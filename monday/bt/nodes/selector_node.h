// monday/bt/nodes/selector_node.h
//
// Selector node for Behavior Tree: ticks children until one succeeds or returns running.

#pragma once

#include <vector>
#include <memory>
#include "monday/bt/bt_engine.h"

namespace monday {

class SelectorNode : public BTNode {
public:
    void add_child(std::shared_ptr<BTNode> child) { children_.push_back(child); }
    NodeStatus tick() override {
        for (auto &child : children_) {
            NodeStatus status = child->tick();
            if (status == NodeStatus::SUCCESS) return NodeStatus::SUCCESS;
        }
        return NodeStatus::FAILURE;
    }
private:
    std::vector<std::shared_ptr<BTNode>> children_;
};

} // namespace monday
