#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <string>

#include "DummySyscalls.cpp"

namespace instrument {
    class InstrumentPass : public llvm::PassInfoMixin<InstrumentPass> {
        public:
            static bool isRequired() {return true;}
            bool instrumentSyscall(llvm::Module &Mod, llvm::FunctionCallee syscallFunc);
            llvm::PreservedAnalyses run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr);
    };

    llvm::PreservedAnalyses InstrumentPass::run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr) {
        llvm::LLVMContext &context = Mod.getContext();
        llvm::Type *i32 = llvm::Type::getInt32Ty(context);
        llvm::FunctionType *syscallFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i32}, false);
        llvm::FunctionCallee syscallFunc = Mod.getOrInsertFunction("dummy_syscall", syscallFuncType);
        bool modified = instrumentSyscall(Mod, syscallFunc);
   
        return modified ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
    }

    bool InstrumentPass::instrumentSyscall(llvm::Module &Mod, llvm::FunctionCallee syscallFunc) {
        bool modified = false;

        llvm::IRBuilder<> Builder(Mod.getContext());
        for (llvm::Function &func : Mod) {
            if(func.getName() == "dummy_syscall")
                continue;

            for(llvm::BasicBlock &bb : func) {
                for(llvm::Instruction &inst : bb) {
                    if(llvm::CallInst *callInst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                        llvm::Function *calledFunc = callInst->getCalledFunction();
                        if(calledFunc) {
                            std::string funcName = calledFunc->getName().str();
                            int syscallArg = libcMap(funcName);
                            if(syscallArg >= 0) {
                                Builder.SetInsertPoint(callInst);
                                llvm::Value *arg = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Mod.getContext()), syscallArg);
                                Builder.CreateCall(syscallFunc, {arg});
                                modified = true;
                            }
                        }
                    }
                }
            }
        }
        return modified;
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "InstrumentPass", "v0.2",
        [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::ModulePassManager &MPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                    if(Name == "instrument-pass") {
                        MPM.addPass(instrument::InstrumentPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}