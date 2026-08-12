// monday/automation/tool_dispatcher.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include "monday/security/consent.h"
#include "monday/security/audit_log.h"
#include "monday/automation/pid_verifier.h"

#include <string>
#include <memory>

namespace monday {

struct ToolCall {
    std::string tool_name;
    ActionClass action_class{ActionClass::READ_ONLY};
    std::string target_scope;
    std::string description;
    int target_pid{0};
    bool is_cherry_scoped{false};
    std::string user_cherry_input;
    std::string parameters_json;
};

struct ToolExecutionResult {
    bool success{true};
    std::string output_json;
    std::string error_message;
};

class ToolDispatcher {
public:
    ToolDispatcher(ConsentManager* consent_mgr = nullptr, bool enable_audit_log = true);
    ~ToolDispatcher() = default;

    void set_session_locked(bool locked) { session_locked_ = locked; }
    bool is_session_locked() const { return session_locked_; }

    Result<ToolExecutionResult> execute_tool(const ToolCall& call);

private:
    ConsentManager* consent_mgr_{nullptr};
    bool enable_audit_log_{true};
    bool session_locked_{false};
};

} // namespace monday
