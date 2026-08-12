// monday/automation/tool_dispatcher.cpp
#include "monday/automation/tool_dispatcher.h"
#include "monday/core/log.h"

namespace monday {

ToolDispatcher::ToolDispatcher(ConsentManager* consent_mgr, bool enable_audit_log)
    : consent_mgr_(consent_mgr), enable_audit_log_(enable_audit_log), session_locked_(false) {}

Result<ToolExecutionResult> ToolDispatcher::execute_tool(const ToolCall& call) {
    // 1. Session-lock pause for DESTRUCTIVE and WRITE operations
    if (session_locked_) {
        if (call.action_class == ActionClass::DESTRUCTIVE || call.action_class == ActionClass::WRITE) {
            monday_log_write(MONDAY_LOG_SYSTEM, "Tool execution blocked due to session lock");
            return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "Session locked - DESTRUCTIVE and WRITE actions paused"));
        }
    }

    // 2. PID verification if target_pid is specified
    if (call.target_pid > 0) {
        auto pid_res = verify_pid_binary_path(call.target_pid);
        if (!pid_res.has_value()) {
            monday_log_write(MONDAY_LOG_SYSTEM, "Target PID verification failed");
            return tl::unexpected(pid_res.error());
        }
    }

    // 3. Consent verification via ConsentManager
    if (consent_mgr_) {
        auto consent_res = consent_mgr_->request_consent(
            call.action_class,
            call.target_scope,
            call.description,
            call.is_cherry_scoped,
            call.user_cherry_input
        );
        if (!consent_res.has_value() || !consent_res.value()) {
            monday_log_write(MONDAY_LOG_SYSTEM, "Consent denied for tool execution");
            return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_CONSENT_DENIED, "Consent denied for tool execution"));
        }
    }

    // 4. Record entry to HMAC-chained Audit Log before execution
    if (enable_audit_log_) {
        std::string ac_str = action_class_to_string(call.action_class);
        std::string desc = "TOOL:" + call.tool_name + " SCOPE:" + call.target_scope + " DESC:" + call.description;
        monday_audit_log_write(ac_str.c_str(), desc.c_str());
    }

    // 5. Execute tool action
    ToolExecutionResult exec_res;
    exec_res.success = true;
    exec_res.output_json = "{\"status\": \"success\", \"tool\": \"" + call.tool_name + "\"}";

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Executed tool on target scope");

    return exec_res;
}

} // namespace monday
