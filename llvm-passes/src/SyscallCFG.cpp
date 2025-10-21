#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constant.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <string>
#include <vector>
#include <utility>
#include <map>
#include <fstream>

#include "../include/libc_callnames.h"
bool isLibcFunction(const std::string &funcName){
    return libc_callnames.find(funcName) != libc_callnames.end();
}

namespace icfg {
    struct Node {
        uint64_t nodeId;
        std::vector<std::pair<uint64_t, std::string>> edges;
    };

    class InstrumentedCFGBuilderPass : public llvm::PassInfoMixin<InstrumentedCFGBuilderPass> {
        public:
            static bool isRequired() {return true;}
            llvm::PreservedAnalyses run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr);
        private:
            void clearGraph();
            uint64_t createNode();
            uint64_t scanCallInstructions(llvm::BasicBlock &bb, llvm::Function &func);
            void dumpGraph(llvm::Module &Mod);
            std::map<llvm::Function*, uint64_t> funcExitNodeId;
            std::map<std::pair<llvm::Function*, llvm::BasicBlock*>, uint64_t> bbId;
            std::map<uint64_t, Node> graphNodes;
            uint64_t nodeCounter = 0;
    };

    void InstrumentedCFGBuilderPass::clearGraph() {
        graphNodes.clear();
        bbId.clear();
        funcExitNodeId.clear();
        nodeCounter = 0;
    }

    uint64_t InstrumentedCFGBuilderPass::createNode() {
        Node newNode;
        newNode.nodeId = nodeCounter;
        graphNodes[newNode.nodeId] = newNode;
        return nodeCounter++;
    }

    uint64_t InstrumentedCFGBuilderPass::scanCallInstructions(llvm::BasicBlock &bb, llvm::Function &func) {
        auto bbKey = std::make_pair(&func, &bb);
        auto entryKey = std::make_pair(&func, &func.getEntryBlock());
        if(bbId.find(bbKey) == bbId.end()) {
            bbId[bbKey] = createNode();
        }
        uint64_t currentNodeId = bbId[bbKey];
        if(bbId.find(entryKey) == bbId.end()) {
            bbId[entryKey] = createNode();
        }
        uint64_t funcEntryNodeId = bbId[entryKey];
        for(llvm::Instruction &inst : bb) {
            if(auto *callInst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                if(llvm::Function *calledFunc = callInst->getCalledFunction()) {
                    std::string funcName = calledFunc->getName().str();
                    if(funcName == func.getName().str()) {
                        graphNodes[currentNodeId].edges.push_back({funcEntryNodeId, "ε"});
                    } else if(funcName == "dummy_syscall") {
                        llvm::Value *arg  = callInst->getArgOperand(0);
                        std::string label;
                        if (auto *constInt = llvm::dyn_cast<llvm::ConstantInt>(arg)) {
                            uint64_t callArg = constInt->getZExtValue();
                            label = "dummy_syscall(" + std::to_string(callArg) + ")";
                        }
                        uint64_t nextNodeId = createNode();
                        graphNodes[currentNodeId].edges.push_back({nextNodeId, label});
                        currentNodeId = nextNodeId;
                    } else if (funcName == "syscall") {
                        std::string label;
                        if (auto* constInt = llvm::dyn_cast<llvm::ConstantInt>(callInst->getArgOperand(0))) {
                            label = "syscall(" + std::to_string(constInt->getZExtValue()) + ")";
                        }
                        uint64_t nextNodeId = createNode();
                        graphNodes[currentNodeId].edges.push_back({nextNodeId, label});
                        currentNodeId = nextNodeId;
                    } else if (calledFunc->isDeclaration()) {
                        uint64_t nextNodeId = createNode();
                        if(isLibcFunction(funcName)){
                            // funcName = funcName + "()";
                            // graphNodes[currentNodeId].edges.push_back({nextNodeId, funcName});
                            continue;
                        }
                        else    
                            graphNodes[currentNodeId].edges.push_back({nextNodeId, "ε"});
                        currentNodeId = nextNodeId;
                    } else {
                        llvm::BasicBlock &calledFuncEntryBB = calledFunc->getEntryBlock();
                        if(bbId.find({calledFunc, &calledFuncEntryBB}) == bbId.end()) {
                            for(llvm::BasicBlock &calleeBB : *calledFunc) {
                                bbId[{calledFunc, &calleeBB}] = createNode();
                            }
                        }
                        uint64_t calledFuncEntryId = bbId.at({calledFunc, &calledFuncEntryBB});
                        // std::string label = calledFunc->getName().str() + "()";
                        graphNodes[currentNodeId].edges.push_back({calledFuncEntryId, "ε"});
                        uint64_t nextNodeId  = createNode();
                        graphNodes[funcExitNodeId.at(calledFunc)].edges.push_back({nextNodeId, "ε"});
                        currentNodeId = nextNodeId;
                    }
                }
            }
        }        
        return currentNodeId;
    }

    llvm::PreservedAnalyses InstrumentedCFGBuilderPass::run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr) {
        clearGraph();
        createNode();

        for(llvm::Function &func : Mod) {
            if(func.isDeclaration()) continue;
            auto entryKey = std::make_pair(&func, &func.getEntryBlock());
            bbId[entryKey] = createNode();
        }

        for(llvm::Function &func : Mod){
            if (func.isDeclaration()) continue;
            uint64_t exitNodeId = createNode();
            funcExitNodeId[&func] = exitNodeId;
        }

        llvm::Function *mainFunc = Mod.getFunction("main");
        uint64_t entryNodeId = bbId.at({mainFunc, &mainFunc->getEntryBlock()});
        graphNodes[0].edges.push_back({entryNodeId, "ε"});

        for(llvm::Function &func : Mod){
            if(func.isDeclaration()) continue;

            for(llvm::BasicBlock &bb : func) {
                uint64_t lastNodeId = scanCallInstructions(bb, func);
                llvm::Instruction *terminator = bb.getTerminator();
                if(!terminator) continue;
                if (llvm::isa<llvm::ReturnInst>(terminator)) {
                    // std::string label = "return:" + func.getName().str() + "()";
                    graphNodes[lastNodeId].edges.push_back({funcExitNodeId.at(&func), "ε"});
                }
                for(unsigned i = 0; i < terminator->getNumSuccessors(); i++) {
                    llvm::BasicBlock *successor = terminator->getSuccessor(i);
                    auto successorKey = std::make_pair(&func, successor);
                    if(bbId.find(successorKey) == bbId.end())
                        bbId[successorKey] = createNode();
                    uint64_t successorNodeId = bbId.at(successorKey);
                    graphNodes[lastNodeId].edges.push_back({successorNodeId, "ε"});
                }
            }
        }

        dumpGraph(Mod);
        return llvm::PreservedAnalyses::all();
    }

    void InstrumentedCFGBuilderPass::dumpGraph(llvm::Module &Mod) {
        std::string filename = "cfg.dot";
        std::ofstream outfile(filename);
        outfile << "digraph CFG {\n";
        outfile << "    rankdir=LR;\n";
        outfile << "    node [shape=circle];\n";
        for(auto const& [id, node] : graphNodes) {
            for(auto const& edge : node.edges) {
                outfile << "    " << id << " -> " << edge.first;
                outfile << " [label=\"" << edge.second << "\"];\n";
            }
        }
        outfile << "}\n";
        outfile.close();
    }
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "InstrumentedCFGBuilderPass", "v0.2",
        [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::ModulePassManager &MPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                    if(Name == "instrumented-cfg-builder-pass") {
                        MPM.addPass(icfg::InstrumentedCFGBuilderPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}
                