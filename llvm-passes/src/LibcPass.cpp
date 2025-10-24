#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Path.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

#include <map>
#include <queue>
#include <string>
#include <utility>
#include <fstream>

#include "CallNames.cpp"
#include "FSM.cpp"

namespace cfg {
    class libcCFGPass : public llvm::PassInfoMixin<libcCFGPass> {
        public:
            static bool isRequired() {return true;}
            llvm::PreservedAnalyses run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr);
        private:
            fsm::nfaNode* startNode;
            uint64_t nodeCounter;
            std::map<llvm::Function*, fsm::nfaNode*> funcExitNode;
            std::map<std::pair<llvm::Function*, llvm::BasicBlock*>, fsm::nfaNode*> bbId;

            fsm::nfaNode* createNode();
            void dumpGraph(llvm::Module &Mod, fsm::nfaNode* startNode);
            fsm::nfaNode* scanCallInstructions(llvm::BasicBlock &bb, llvm::Function &func);
    };

    fsm::nfaNode* libcCFGPass::createNode() {
        fsm::nfaNode* newNode = new fsm::nfaNode(nodeCounter++, false);
        return newNode;
    }

    fsm::nfaNode* libcCFGPass::scanCallInstructions(llvm::BasicBlock &bb, llvm::Function &func) {
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
                    } else if(calledFunc->isDeclaration()) {
                        fsm::nfaNode* nextNode = createNode();
                        if(isLibcFunction(funcName)){
                            funcName = "call:" + funcName;
                            currentNode->edges.push_back({nextNode, funcName});
                            if (funcName == "call:exit" || funcName == "call:_exit" || 
                                funcName == "call:quick_exit" || funcName == "call:abort") {
                                nextNode->isFinalState = true;
                            }
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
                        std::string label = "call:" + calledFunc->getName().str();
                        currentNode->edges.push_back({calledFuncEntryNode, label});
                        fsm::nfaNode* nextNode = createNode();
                        funcExitNode.at(calledFunc)->edges.push_back({nextNode, "ε"});
                        currentNode = nextNode;
                    }
                }
            }
        }
        return currentNode;
    }

    void libcCFGPass::dumpGraph(llvm::Module &Mod, fsm::nfaNode* startNode) {
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

    llvm::PreservedAnalyses libcCFGPass::run(llvm::Module &Mod, llvm::AnalysisManager<llvm::Module> &mngr) {
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
                fsm::nfaNode* lastNodeId = scanCallInstructions(bb, func);
                llvm::Instruction *terminator = bb.getTerminator();
                if(!terminator) continue;
                if (llvm::isa<llvm::ReturnInst>(terminator)) {
                    std::string label = "ret:" + func.getName().str();
                    lastNodeId->edges.push_back({funcExitNode.at(&func), label});
                }
                for(unsigned i = 0; i < terminator->getNumSuccessors(); i++) {
                    llvm::BasicBlock *successor = terminator->getSuccessor(i);
                    auto successorKey = std::make_pair(&func, successor);
                    if(bbId.find(successorKey) == bbId.end())
                        bbId[successorKey] = createNode();
                    fsm::nfaNode* successorNode = bbId.at(successorKey);
                    lastNodeId->edges.push_back({successorNode, "ε"});
                }
            }
        }

        fsm::removeEpsilonTransitions(startNode);

        fsm::nfaNode* mergedStartNode = fsm::mergeEquivalentStates(startNode);

        dumpGraph(Mod, mergedStartNode);
        
        fsm::clearGraph(mergedStartNode);

        return llvm::PreservedAnalyses::all();
    }
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "libcCFGPass", "v0.4",
        [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::ModulePassManager &MPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                    if(Name == "libc-cfg-pass") {
                        MPM.addPass(cfg::libcCFGPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}