// monday/bt/nodes/action_node.h
//
// Action node for Behavior Tree: dispatches GOAP intent action.

#pragma once

#include <string>
#include "monday/bt/bt_engine.h"
#include "monday/goap/goap_planner.h"

namespace monday {

class ActionNode : public BTNode {
public:
    ActionNode(GOAPPlanner *planner, std::string query)
        : planner_(planner), query_(std::move(query)) {}

    NodeStatus tick() override;

private:
    GOAPPlanner *planner_;
    std::string  query_;
};

} // namespace monday
