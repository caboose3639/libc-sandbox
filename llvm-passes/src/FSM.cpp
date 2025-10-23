#include "../include/FSM.h"

std::set<fsm::nfaNode*> fsm::epsilonClosure(fsm::nfaNode* node) {
    std::set<fsm::nfaNode*> closure;
    std::queue<fsm::nfaNode*> q;

    q.push(node);
    closure.insert(node);

    while(!q.empty()) {
        fsm::nfaNode* currentNode = q.front();
        q.pop();
        for(auto const& edge : currentNode->edges) {
            if(edge.second == "ε") {
                if(closure.find(edge.first) == closure.end()) {
                    closure.insert(edge.first);
                    q.push(edge.first);
                }
            }
        }
    }
    return closure;
}   

void fsm::removeEpsilonTransitions(fsm::nfaNode* startNode) {
    std::set<fsm::nfaNode*> allNodes;
    std::queue<fsm::nfaNode*> q;

    q.push(startNode);
    allNodes.insert(startNode);

    while(!q.empty()) {
        fsm::nfaNode* currentNode = q.front();
        q.pop();
        for(auto const& edge : currentNode->edges) {
            if(allNodes.find(edge.first) == allNodes.end()) {
                allNodes.insert(edge.first);
                q.push(edge.first);
            }
        }
    }

    for(fsm::nfaNode* node : allNodes) {
        std::set<fsm::nfaNode*> closure = fsm::epsilonClosure(node);
        std::vector<std::pair<fsm::nfaNode*, std::string>> newEdges;
        for(fsm::nfaNode* closureNode : closure) {
            for(auto const& edge : closureNode->edges){
                if(edge.second != "ε") {
                    newEdges.push_back(edge);
                }
            }
        }
        node->edges.assign(newEdges.begin(), newEdges.end());
    }
}

void fsm::clearGraph(fsm::nfaNode* startNode) {

    std::set<fsm::nfaNode*> allNodes;
    std::queue<fsm::nfaNode*> q;

    q.push(startNode);
    allNodes.insert(startNode);

    while (!q.empty()) {
        fsm::nfaNode* currentNode = q.front();
        q.pop();

        for (const auto& edge : currentNode->edges) {
            fsm::nfaNode* neighbor = edge.first;
            if (neighbor != nullptr)
                if (allNodes.insert(neighbor).second) {
                    q.push(neighbor);
                }
            }
        }

    for(fsm::nfaNode* node : allNodes) {
        delete node;
    }
}