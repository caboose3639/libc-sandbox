#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <queue>
#include <stack>
#include <map>
#include <cstdint>
#include <numeric>
#include <algorithm>
#include <memory>

namespace fsm {
    struct nfaNode {
        uint64_t nodeId;
        bool isFinalState;
        std::vector<std::pair<nfaNode*, std::string>> edges;

        nfaNode(uint64_t id, bool isFinal) : nodeId(id), isFinalState(isFinal) {}
    };

    std::set<nfaNode*> epsilonClosure(nfaNode* node);

    void removeEpsilonTransitions(nfaNode* startNode);

    nfaNode* mergeEquivalentStates(nfaNode* startNode);

    void clearGraph(nfaNode* startNode);
}
