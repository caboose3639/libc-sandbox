#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Path.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

#include <map>
#include <string>
#include <utility>
#include <fstream>

#include "CallNames.cpp"
#include "FSM.cpp"

namespace icfg {
    class syscallCFGPass : public llvm::PassInfoMixin<syscallCFGPass> {
        public:
            static bool isRequired() {return true;}
            llvm::PreservedAnalyses run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr);
        private:
            fsm::nfaNode* startNode;
            uint64_t nodeCounter;
            std::map<llvm::Function*, fsm::nfaNode*> funcExitNode;
            std::map<std::pair<llvm::Function*, llvm::BasicBlock*>, fsm::nfaNode*> bbId;

            fsm::nfaNode* createNode();
            void dumpGraph(llvm::Module &Mod);
            fsm::nfaNode* scanCallInstructions(llvm::BasicBlock &bb, llvm::Function &func);
    };

    fsm::nfaNode* syscallCFGPass::createNode() {
        fsm::nfaNode* newNode = new fsm::nfaNode(nodeCounter++, false);
        return newNode;
    }

    fsm::nfaNode* syscallCFGPass::scanCallInstructions(llvm::BasicBlock &bb, llvm::Function &func) {
        auto bbKey = std::make_pair(&func, &bb);
        auto entryKey = std::make_pair(&func, &func.getEntryBlock());
        if(bbId.find(bbKey) == bbId.end()) {
            bbId[bbKey] = createNode();
        }
        fsm::nfaNode* currentNode = bbId[bbKey];
        if(bbId.find(entryKey) == bbId.end()) {
            bbId[entryKey] = createNode();
        }
        fsm::nfaNode* funcEntryNode = bbId[entryKey];
        for(llvm::Instruction &inst : bb) {
            if(auto *callInst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                if(llvm::Function *calledFunc = callInst->getCalledFunction()) {
                    std::string funcName = calledFunc->getName().str();
                    if(funcName == func.getName().str()) {
                        currentNode->edges.push_back({funcEntryNode, "ε"});
                    } else if(funcName == "dummy_syscall") {
                        llvm::Value *arg  = callInst->getArgOperand(0);
                        std::string label;
                        if (auto *constInt = llvm::dyn_cast<llvm::ConstantInt>(arg)) {
                            uint64_t callArg = constInt->getZExtValue();
                            label = "dummy_syscall(" + std::to_string(callArg) + ")";
                        }
                        fsm::nfaNode* nextNode = createNode();
                        currentNode->edges.push_back({nextNode, label});
                        currentNode = nextNode;
                    } else if (funcName == "syscall") {
                        std::string label;
                        if (auto* constInt = llvm::dyn_cast<llvm::ConstantInt>(callInst->getArgOperand(0))) {
                            label = "syscall(" + std::to_string(constInt->getZExtValue()) + ")";
                        }
                        fsm::nfaNode* nextNode = createNode();
                        currentNode->edges.push_back({nextNode, label});
                        currentNode = nextNode;
                    } else if (calledFunc->isDeclaration()) {
                        fsm::nfaNode* nextNode = createNode();
                        if(isLibcFunction(funcName)){
                            if (funcName == "call:exit" || funcName == "call:_exit" || 
                                funcName == "call:quick_exit" || funcName == "call:abort") {
                                nextNode->isFinalState = true;
                            }
                            continue;
                        }
                        else
                            currentNode->edges.push_back({nextNode, "ε"});
                        currentNode = nextNode;
                    } else {
                        llvm::BasicBlock &calledFuncEntryBB = calledFunc->getEntryBlock();
                        if(bbId.find({calledFunc, &calledFuncEntryBB}) == bbId.end()) {
                            for(llvm::BasicBlock &calleeBB : *calledFunc) {
                                bbId[{calledFunc, &calleeBB}] = createNode();
                            }
                        }
                        fsm::nfaNode* calledFuncEntryNode = bbId.at({calledFunc, &calledFuncEntryBB});
                        currentNode->edges.push_back({calledFuncEntryNode, "ε"});
                        fsm::nfaNode* nextNode = createNode();
                        funcExitNode.at(calledFunc)->edges.push_back({nextNode, "ε"});
                        currentNode = nextNode;
                    }
                }
            }
        }
        return currentNode;
    }

    void syscallCFGPass::dumpGraph(llvm::Module &Mod) {
        fsm::removeEpsilonTransitions(startNode);

        std::set<fsm::nfaNode*> visited;
        std::queue<fsm::nfaNode*> q;
        q.push(startNode);
        visited.insert(startNode);

        std::string sourceFile = Mod.getSourceFileName();
        llvm::StringRef baseNameRef = llvm::sys::path::stem(sourceFile);
        std::string baseName = baseNameRef.str();
        std::string filename = baseName + "_cfg.dot";
        std::ofstream outfile(filename);
        outfile << "digraph CFG {\n";
        outfile << "    rankdir=LR;\n";
        outfile << "    node [shape=circle];\n";

        while(!q.empty()) {
            fsm::nfaNode* currentNode = q.front();
            q.pop();
            for(auto const& edge : currentNode->edges) {
                outfile << "    " << currentNode->nodeId << " -> " << edge.first->nodeId << " [label=\"" << edge.second << "\"];" << std::endl;
                fsm::nfaNode* neighbor = edge.first;
                if(visited.find(neighbor) == visited.end()) {
                    visited.insert(neighbor);
                    q.push(neighbor);
                }
            }
        }

        outfile << "}\n";
        outfile.close();
    }

    llvm::PreservedAnalyses syscallCFGPass::run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr) {
        startNode = createNode();

        for(llvm::Function &func : Mod) {
            if(func.isDeclaration()) continue;
            auto entryKey = std::make_pair(&func, &func.getEntryBlock());
            bbId[entryKey] = createNode();
        }

        for(llvm::Function &func : Mod){
            if (func.isDeclaration()) continue;
            fsm::nfaNode* exitNode = createNode();
            funcExitNode[&func] = exitNode;
        }

        llvm::Function *mainFunc = Mod.getFunction("main");
        if (funcExitNode.count(mainFunc)) {
            funcExitNode.at(mainFunc)->isFinalState = true;
        }
        fsm::nfaNode* entryNode = bbId.at({mainFunc, &mainFunc->getEntryBlock()});
        startNode->edges.push_back({entryNode, "ε"});

        for(llvm::Function &func : Mod){
            if(func.isDeclaration()) continue;

            for(llvm::BasicBlock &bb : func) {
                fsm::nfaNode* lastNode = scanCallInstructions(bb, func);
                llvm::Instruction *terminator = bb.getTerminator();
                if(!terminator) continue;
                if (llvm::isa<llvm::ReturnInst>(terminator)) {
                    lastNode->edges.push_back({funcExitNode.at(&func), "ε"});
                }
                for(unsigned i = 0; i < terminator->getNumSuccessors(); i++) {
                    llvm::BasicBlock *successor = terminator->getSuccessor(i);
                    auto successorKey = std::make_pair(&func, successor);
                    if(bbId.find(successorKey) == bbId.end())
                        bbId[successorKey] = createNode();
                    fsm::nfaNode* successorNode = bbId.at(successorKey);
                    lastNode->edges.push_back({successorNode, "ε"});
                }
            }
        }

        dumpGraph(Mod);
        fsm::clearGraph(startNode);
        return llvm::PreservedAnalyses::all();
    }
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "syscallCFGPass", "v0.3",
        [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::ModulePassManager &MPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                    if(Name == "syscall-cfg-pass") {
                        MPM.addPass(icfg::syscallCFGPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}
                