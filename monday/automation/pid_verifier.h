// monday/automation/pid_verifier.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>

namespace monday {

// Resolves and verifies the binary path of a process by PID.
// Checks the resolved path against the security blocklist.
// Returns:
// - MONDAY_ERR_INVALID_ARG for pid <= 0
// - MONDAY_ERR_NOT_FOUND for non-existent pid
// - MONDAY_ERR_AUTOMATION_BLOCKED if process is in blocklist
// - Verified canonical binary path string on success
Result<std::string> verify_pid_binary_path(int pid);

} // namespace monday
