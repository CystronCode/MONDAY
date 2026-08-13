// monday/automation/tool_dispatcher.cpp
//
// Tool Dispatcher — enforces consent, PID verification, audit logging, and
// executes the concrete action. Per Architecture §2.5 and §4 Step 6:
//   "Tool Dispatcher classifies the action. LLM output is untrusted input."
//
// Windows-specific handlers: ShellExecuteW for AUTOMATION_LAUNCH_APP,
// TerminateProcess + EnumProcesses for AUTOMATION_CLOSE_PROCESS,
// and GetLocalTime for SYSTEM_GET_TIME / SYSTEM_GET_DATE.

#include "monday/automation/tool_dispatcher.h"
#include "monday/core/log.h"

#include <string>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Psapi.lib")
#endif

namespace monday {

ToolDispatcher::ToolDispatcher(ConsentManager* consent_mgr, bool enable_audit_log)
    : consent_mgr_(consent_mgr), enable_audit_log_(enable_audit_log), session_locked_(false) {}

// ── Internal action handlers ──────────────────────────────────────────────────

static ToolExecutionResult handle_get_time() {
    ToolExecutionResult res;
    res.success = true;
    time_t now = time(nullptr);
    struct tm *tm_info = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    res.output_json = std::string("{\"time\":\"") + buf + "\"}";
    res.error_message = std::string("The current time is ") + buf;
    return res;
}

static ToolExecutionResult handle_get_date() {
    ToolExecutionResult res;
    res.success = true;
    time_t now = time(nullptr);
    struct tm *tm_info = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%A, %d %B %Y", tm_info);
    res.output_json = std::string("{\"date\":\"") + buf + "\"}";
    res.error_message = std::string("Today is ") + buf;
    return res;
}

#ifdef _WIN32
// Maps a logical app name to a Windows executable path or command
static std::wstring resolve_app_path(const std::string& app_name) {
    std::string lower = app_name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("chrome")   != std::string::npos) return L"chrome.exe";
    if (lower.find("edge")     != std::string::npos) return L"msedge.exe";
    if (lower.find("notepad")  != std::string::npos) return L"notepad.exe";
    if (lower.find("calc")     != std::string::npos) return L"calc.exe";
    if (lower.find("explorer") != std::string::npos) return L"explorer.exe";
    if (lower.find("setting")  != std::string::npos) return L"ms-settings:";
    if (lower.find("task manager") != std::string::npos || lower.find("taskmgr") != std::string::npos)
        return L"taskmgr.exe";
    if (lower.find("cmd")      != std::string::npos) return L"cmd.exe";
    if (lower.find("powershell") != std::string::npos) return L"powershell.exe";
    if (lower.find("paint")    != std::string::npos) return L"mspaint.exe";
    if (lower.find("word")     != std::string::npos) return L"winword.exe";
    if (lower.find("excel")    != std::string::npos) return L"excel.exe";

    // Fallback: treat literal input as the executable name
    return std::wstring(app_name.begin(), app_name.end());
}

static ToolExecutionResult handle_launch_app(const std::string& app_name) {
    ToolExecutionResult res;
    std::wstring exe = resolve_app_path(app_name);

    HINSTANCE hInst = ShellExecuteW(NULL, L"open", exe.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)hInst > 32) {
        res.success = true;
        res.output_json = "{\"status\":\"launched\"}";
        res.error_message = "Launched: " + app_name;
    } else {
        res.success = false;
        res.error_message = "Failed to launch: " + app_name
                          + " (ShellExecute error " + std::to_string((intptr_t)hInst) + ")";
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, res.error_message.c_str());
    }
    return res;
}

static ToolExecutionResult handle_close_process(const std::string& process_name) {
    ToolExecutionResult res;
    std::string lower = process_name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Normalise: add .exe if missing
    std::string target_exe = lower;
    if (target_exe.size() < 4 || target_exe.substr(target_exe.size() - 4) != ".exe") {
        target_exe += ".exe";
    }

    // Map friendly names
    if (target_exe.find("task manager") != std::string::npos) target_exe = "taskmgr.exe";
    if (target_exe.find("chrome")       != std::string::npos) target_exe = "chrome.exe";
    if (target_exe.find("edge")         != std::string::npos) target_exe = "msedge.exe";
    if (target_exe.find("notepad")      != std::string::npos) target_exe = "notepad.exe";

    // Enumerate running processes
    DWORD pids[1024];
    DWORD bytes_returned = 0;
    if (!EnumProcesses(pids, sizeof(pids), &bytes_returned)) {
        res.success = false;
        res.error_message = "EnumProcesses failed";
        return res;
    }

    DWORD count = bytes_returned / sizeof(DWORD);
    int killed = 0;

    for (DWORD i = 0; i < count; i++) {
        if (pids[i] == 0) continue;
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pids[i]);
        if (!hProcess) continue;

        char exe_path[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameA(hProcess, 0, exe_path, &size)) {
            std::string exe_str(exe_path);
            std::transform(exe_str.begin(), exe_str.end(), exe_str.begin(), ::tolower);

            // Match only the filename portion
            size_t sep = exe_str.find_last_of("\\/");
            std::string exe_base = (sep != std::string::npos) ? exe_str.substr(sep + 1) : exe_str;

            if (exe_base == target_exe) {
                TerminateProcess(hProcess, 1);
                killed++;
            }
        }
        CloseHandle(hProcess);
    }

    if (killed > 0) {
        res.success = true;
        res.output_json = "{\"status\":\"terminated\",\"count\":" + std::to_string(killed) + "}";
        res.error_message = "Terminated " + std::to_string(killed) + " instance(s) of: " + process_name;
    } else {
        res.success = false;
        res.error_message = "No running process found matching: " + process_name;
    }
    return res;
}
#else
// Linux stubs — automation is handled via AT-SPI2 in the full Phase 4 build
static ToolExecutionResult handle_launch_app(const std::string& app_name) {
    ToolExecutionResult res;
    res.success = false;
    res.error_message = "AUTOMATION_LAUNCH_APP: Linux AT-SPI2 path not yet implemented (Phase 4)";
    return res;
}
static ToolExecutionResult handle_close_process(const std::string& process_name) {
    ToolExecutionResult res;
    res.success = false;
    res.error_message = "AUTOMATION_CLOSE_PROCESS: Linux AT-SPI2 path not yet implemented (Phase 4)";
    return res;
}
#endif

// ── Volume stubs (Windows Core Audio / Phase 4) ───────────────────────────────

static ToolExecutionResult handle_volume(bool up) {
    ToolExecutionResult res;
#ifdef _WIN32
    // Simulate a media key press via SendInput
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = up ? VK_VOLUME_UP : VK_VOLUME_DOWN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = up ? VK_VOLUME_UP : VK_VOLUME_DOWN;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
    res.success = true;
    res.output_json = up ? "{\"status\":\"volume_up\"}" : "{\"status\":\"volume_down\"}";
    res.error_message = up ? "Volume increased." : "Volume decreased.";
#else
    res.success = false;
    res.error_message = "Volume control not implemented on this platform.";
#endif
    return res;
}

// ── Main dispatch ─────────────────────────────────────────────────────────────

Result<ToolExecutionResult> ToolDispatcher::execute_tool(const ToolCall& call) {
    // 1. Session-lock check
    if (session_locked_) {
        if (call.action_class == ActionClass::DESTRUCTIVE || call.action_class == ActionClass::WRITE) {
            monday_log_write(MONDAY_LOG_SYSTEM, "Tool execution blocked due to session lock");
            return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED,
                                               "Session locked — DESTRUCTIVE/WRITE actions paused"));
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

    // 3. Consent verification
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
            return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_CONSENT_DENIED, "Consent denied"));
        }
    }

    // 4. HMAC-chained audit log — record BEFORE execution
    if (enable_audit_log_) {
        std::string ac_str = action_class_to_string(call.action_class);
        std::string desc = "TOOL:" + call.tool_name
                         + " SCOPE:" + call.target_scope
                         + " DESC:" + call.description;
        monday_audit_log_write(ac_str.c_str(), desc.c_str());
    }

    // 5. Action dispatch — concrete handlers per action name
    const std::string& action = call.tool_name;

    if (action == "SYSTEM_GET_TIME")          return handle_get_time();
    if (action == "SYSTEM_GET_DATE")          return handle_get_date();
    if (action == "SYSTEM_VOLUME_UP")         return handle_volume(true);
    if (action == "SYSTEM_VOLUME_DOWN")       return handle_volume(false);
    if (action == "AUTOMATION_LAUNCH_APP")    return handle_launch_app(call.parameters_json);
    if (action == "AUTOMATION_CLOSE_PROCESS") return handle_close_process(call.parameters_json);

    // Unimplemented but known actions — return stub success for development
    ToolExecutionResult stub;
    stub.success = true;
    stub.output_json = "{\"status\":\"stub\",\"action\":\"" + action + "\"}";
    stub.error_message = "[Action '" + action + "' dispatched — handler in progress]";
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, stub.error_message.c_str());
    return stub;
}

} // namespace monday
