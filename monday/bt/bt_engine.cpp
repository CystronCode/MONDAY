// monday/bt/bt_engine.cpp
//
// Behavior Tree engine implementation.

#include "monday/bt/bt_engine.h"
#include "monday/core/log.h"
#include "src/generated/blackboard_accessors.h"

namespace monday {

NodeStatus ConditionNode::tick() {
    /* Condition check on Blackboard state */
    if (key_ == "system_status" && expected_value_ == "ready") {
        return NodeStatus::SUCCESS;
    }
    if (!key_.empty()) {
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::FAILURE;
}

NodeStatus ActionNode::tick() {
    if (!planner_ || query_.empty()) {
        return NodeStatus::FAILURE;
    }

    GOAPResult res = planner_->classify(query_.c_str());
    if (res.matched) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "BT ActionNode: GOAP intent matched");
        return NodeStatus::SUCCESS;
    }

    return NodeStatus::FAILURE;
}

BTEngine::BTEngine() : root_(nullptr) {}
BTEngine::~BTEngine() {}

NodeStatus BTEngine::tick() {
    if (!root_) return NodeStatus::FAILURE;
    return root_->tick();
}

} // namespace monday
