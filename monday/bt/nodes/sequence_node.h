// monday/bt/nodes/sequence_node.h
//
// Sequence node for Behavior Tree: ticks children until one fails or returns running.

#pragma once

#include <vector>
#include <memory>
#include "monday/bt/bt_engine.h"

namespace monday {

class SequenceNode : public BTNode {
public:
    void add_child(std::shared_ptr<BTNode> child) { children_.push_back(child); }
    NodeStatus tick() override {
        for (auto &child : children_) {
            NodeStatus status = child->tick();
            if (status != NodeStatus::SUCCESS) return status;
        }
        return NodeStatus::SUCCESS;
    }
private:
    std::vector<std::shared_ptr<BTNode>> children_;
};

} // namespace monday
