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

namespace cfg {

    struct Node {
    uint64_t id;
    std::vector<std::pair<Node*, std::string>> edges;
    };

    class CFGBuilderPass : public llvm::PassInfoMixin<CFGBuilderPass> {
        public:
            llvm::PreservedAnalyses run(llvm::Function &func, llvm::FunctionAnalysisManager &mngr);
        private:
            std::map<uint64_t, Node> graphNodes;
            uint64_t nodeCounter = 0;
    };

    llvm::PreservedAnalyses CFGBuilderPass::run(llvm::Function &func, llvm::FunctionAnalysisManager &mngr) {
        graphNodes.clear();
        nodeCounter = 0;
    }
}