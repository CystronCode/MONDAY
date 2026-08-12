// monday/bt/bt_engine.h
//
// Behavior Tree engine for multi-step action orchestration (§2.5).

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>
#include <string>

#include "monday/core/errors.h"
#include "monday/goap/goap_planner.h"

namespace monday {

enum class NodeStatus {
    SUCCESS = 0,
    FAILURE = 1,
    RUNNING = 2
};

class BTNode {
public:
    virtual ~BTNode() = default;
    virtual NodeStatus tick() = 0;
};

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

class ConditionNode : public BTNode {
public:
    ConditionNode(std::string key, std::string expected_value)
        : key_(std::move(key)), expected_value_(std::move(expected_value)) {}

    NodeStatus tick() override;

private:
    std::string key_;
    std::string expected_value_;
};

class ActionNode : public BTNode {
public:
    ActionNode(GOAPPlanner *planner, std::string query)
        : planner_(planner), query_(std::move(query)) {}

    NodeStatus tick() override;

private:
    GOAPPlanner *planner_;
    std::string  query_;
};

class BTEngine {
public:
    BTEngine();
    ~BTEngine();

    void set_root(std::shared_ptr<BTNode> root) { root_ = root; }
    NodeStatus tick();

private:
    std::shared_ptr<BTNode> root_;
};

} // namespace monday
