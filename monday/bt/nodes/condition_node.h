// monday/bt/nodes/condition_node.h
//
// Condition node for Behavior Tree: verifies Blackboard state.

#pragma once

#include <string>
#include "monday/bt/bt_engine.h"

namespace monday {

class ConditionNode : public BTNode {
public:
    ConditionNode(std::string key, std::string expected_value)
        : key_(std::move(key)), expected_value_(std::move(expected_value)) {}

    NodeStatus tick() override;

private:
    std::string key_;
    std::string expected_value_;
};

} // namespace monday
