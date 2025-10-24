#include "../include/FSM.h"
#include <cstdint>
#include <queue>

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

fsm::nfaNode* fsm::mergeEquivalentStates(fsm::nfaNode* startNode) {
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

    std::map<std::set<fsm::nfaNode*>, fsm::nfaNode*> stateMap;
    std::queue<std::set<fsm::nfaNode*>> stateQueue;
    uint64_t newId = 0;
    fsm::nfaNode* newStartNode = nullptr;

    std::set<fsm::nfaNode*> startSet = {startNode};
    newStartNode = new fsm::nfaNode(newId++, startNode->isFinalState);

    stateMap[startSet] = newStartNode;
    stateQueue.push(startSet);

    while (!stateQueue.empty()) {
        std::set<fsm::nfaNode*> currentSet = stateQueue.front();
        stateQueue.pop();
        fsm::nfaNode* currentNewNode = stateMap[currentSet];

        std::map<std::string, std::set<fsm::nfaNode*>> transitionMap;
        bool isCurrentSetFinal = false;

        for (fsm::nfaNode* node: currentSet) {
            if(node->isFinalState) {
                isCurrentSetFinal = true;
            }
            for (const auto& edge : node->edges) {
                if(edge.first != nullptr) {
                    transitionMap[edge.second].insert(edge.first);
                }
            }
        }

        currentNewNode->isFinalState = isCurrentSetFinal;

        for(auto const& transition : transitionMap) {
            std::string label = transition.first;
            std::set<fsm::nfaNode*> targetSet = transition.second;

            if(targetSet.empty()) {
                continue;
            }

            fsm::nfaNode* targetNewNode;
            if(stateMap.find(targetSet) == stateMap.end()) {
                targetNewNode = new fsm::nfaNode(newId++, false);
                stateMap[targetSet] = targetNewNode;
                stateQueue.push(targetSet);
            } else {
                targetNewNode = stateMap[targetSet];
            }

            currentNewNode->edges.emplace_back(targetNewNode, label);
        }
    }

    for(fsm::nfaNode* node : allNodes) {
        delete node;
    }

    return newStartNode;
}