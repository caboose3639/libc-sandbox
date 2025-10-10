#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <string>
#include <vector>
#include <utility>
#include <map>
#include <fstream>

#include "../include/libc_callnames.h"

namespace cfg {
    struct Node {
    uint64_t nodeId;
    std::vector<std::pair<uint64_t, std::string>> edges;
    };

    class CFGBuilderPass : public llvm::PassInfoMixin<CFGBuilderPass> {
        public:
            static bool isRequired() {return true;}
            llvm::PreservedAnalyses run(llvm::Function &func, llvm::FunctionAnalysisManager &mngr);
        private:
            void clearGraph();
            void createNode();
            uint64_t scanCallInstructions(llvm::BasicBlock &bb, llvm::Function &func);
            void dumpGraph(llvm::Function &func, uint64_t exitNodeId);
            std::map<llvm::BasicBlock*, uint64_t> bbId;
            std::map<uint64_t, Node> graphNodes;
            uint64_t nodeCounter = 0;
    };

    void CFGBuilderPass::clearGraph() {
        graphNodes.clear();
        bbId.clear();
        nodeCounter = 0;
    }

    void CFGBuilderPass::createNode() {
        Node newNode;
        newNode.nodeId = nodeCounter++;
        graphNodes[newNode.nodeId] = newNode;
    }

    uint64_t CFGBuilderPass::scanCallInstructions(llvm::BasicBlock &bb, llvm::Function &func) {
        uint64_t currentNodeId = bbId[&bb];
        for(llvm::Instruction &inst : bb) {
            if (auto *callInst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                if(llvm::Function *calledFunc = callInst->getCalledFunction()) {
                    std::string funcName = calledFunc->getName().str();
                    if (funcName == func.getName().str()) {
                        graphNodes[currentNodeId].edges.push_back({0, funcName});
                    } else {
                        uint64_t nextNodeId = nodeCounter;
                        createNode();
                        graphNodes[currentNodeId].edges.push_back({nextNodeId, funcName});
                        currentNodeId = nextNodeId;
                    }
                }
            }
        }        
        return currentNodeId;
    }

    llvm::PreservedAnalyses CFGBuilderPass::run(llvm::Function &func, llvm::FunctionAnalysisManager &mngr) {
        clearGraph();
        createNode();

        for(llvm::BasicBlock &bb : func) {
            createNode();
            bbId[&bb] = nodeCounter - 1;
        }

        llvm::BasicBlock &entryBB = func.getEntryBlock();
        uint64_t entryNodeId = bbId[&entryBB];
        graphNodes[0].edges.push_back({entryNodeId, "ε"});

        for(llvm::BasicBlock &bb : func) {
            uint64_t lastNodeId = scanCallInstructions(bb, func);

            llvm::Instruction *terminator = bb.getTerminator();
            for (unsigned i = 0; i < terminator->getNumSuccessors(); i++) {
                llvm::BasicBlock *successor = terminator->getSuccessor(i);
                uint64_t successorNodeId = bbId[successor];
                graphNodes[lastNodeId].edges.push_back({successorNodeId, "ε"});
            }
        }

        std::vector<uint64_t> lastNodeIds;
        for(auto const& [id, node] : graphNodes) {
            if(node.edges.empty() && id != 0) {
                lastNodeIds.push_back(id);
            }
        }

        uint64_t exitNodeId = nodeCounter;
        createNode();

        for(uint64_t id : lastNodeIds) {
            graphNodes[id].edges.push_back({exitNodeId, "ε"});
        }

        uint64_t finalNodeId = nodeCounter - 1;
        dumpGraph(func, finalNodeId);

        return llvm::PreservedAnalyses::all();
    }

    void CFGBuilderPass::dumpGraph(llvm::Function &func, uint64_t exitNodeId) {
        std::string filename = func.getName().str() + "_cfg.dot";
        std::ofstream outfile(filename);
        outfile << "digraph " << func.getName().str() << " {\n";
        outfile << "    rankdir=LR;\n";
        outfile << "    node [shape=circle];\n";
        outfile << "    " << 0 << " [shape=doublecircle, label=\"Start\"];\n";
        outfile << "    " << exitNodeId << " [shape=doublecircle, label=\"End\"];\n";

        for(auto const& [id, node] : graphNodes) {
            for (auto const& edge : node.edges) {
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
        LLVM_PLUGIN_API_VERSION, "CFGBuilderPass", "v0.1",
        [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                    if (Name == "cfg-builder-pass") {
                        FPM.addPass(cfg::CFGBuilderPass());
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}