#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

#include <string>

#include "DummySyscalls.cpp"

namespace instrument {
    class InstrumentPass : public llvm::PassInfoMixin<InstrumentPass> {
        public:
            static bool isRequired() {return true;}
            bool instrumentSyscall(llvm::Module &Mod, llvm::InlineAsm *syscallAsm);
            llvm::PreservedAnalyses run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr);
    };

    llvm::PreservedAnalyses InstrumentPass::run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr) {
        llvm::LLVMContext &context = Mod.getContext();
        llvm::Type *i64 = llvm::Type::getInt64Ty(context);
        llvm::FunctionType *syscallFuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {i64}, false);
        std::string asmString = "mov rax, 470; syscall";
        std::string constraints = "D,~{rax},~{rcx},~{r11},~{memory}";

        llvm::InlineAsm *syscallAsm = llvm::InlineAsm::get(syscallFuncType, asmString, constraints, true);
        bool modified = instrumentSyscall(Mod, syscallAsm);
   
        return modified ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
    }

    bool InstrumentPass::instrumentSyscall(llvm::Module &Mod, llvm::InlineAsm *syscallAsm) {
        bool modified = false;
        std::vector<std::pair<llvm::CallInst*, int>> insertTargets;

        for (llvm::Function &func : Mod) {
            if (func.isDeclaration() || func.isIntrinsic()) continue;

            for (llvm::BasicBlock &bb : func) {
                for (llvm::Instruction &inst : bb) {
                    if (auto *callInst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                        if (llvm::Function *calledFunc = callInst->getCalledFunction()) {
                            std::string funcName = calledFunc->getName().str();
                            int syscallArg = libcMap(funcName);
                            if (syscallArg >= 0)
                                insertTargets.push_back(std::make_pair(callInst, syscallArg));
                        }
                    }
                }
            }
        }

        for (auto &[callInst, syscallArg] : insertTargets) {
            llvm::IRBuilder<> Builder(callInst);
            llvm::Value *arg_i64 =
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(Mod.getContext()), syscallArg, true);
            Builder.CreateCall(syscallAsm, {arg_i64});
            modified = true;
        }

        return modified;
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "InstrumentPass", "v0.4",
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