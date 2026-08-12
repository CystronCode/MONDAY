// monday/automation/pid_verifier.cpp
#include "monday/automation/pid_verifier.h"
#include "monday/automation/automation_blocklist.h"
#include "monday/core/platform.h"

#include <vector>
#include <array>
#include <cstring>
#include <cerrno>

#if defined(__linux__)
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

namespace monday {

Result<std::string> verify_pid_binary_path(int pid) {
    if (pid <= 0) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "Invalid PID <= 0"));
    }

    std::string binary_path;

#if defined(__linux__)
    std::string proc_path = "/proc/" + std::to_string(pid) + "/exe";
    std::array<char, 4096> buf;
    ssize_t len = ::readlink(proc_path.c_str(), buf.data(), buf.size() - 1);
    if (len < 0) {
        if (errno == ENOENT) {
            return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "PID does not exist"));
        }
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "Failed to read binary path from /proc"));
    }
    buf[static_cast<size_t>(len)] = '\0';
    binary_path = buf.data();

#elif defined(_WIN32)
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!hProcess) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "PID does not exist or process handle inaccessible"));
    }
    std::array<char, MAX_PATH> buf;
    DWORD size = static_cast<DWORD>(buf.size());
    if (QueryFullProcessImageNameA(hProcess, 0, buf.data(), &size) == 0) {
        CloseHandle(hProcess);
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "Failed to query process image name"));
    }
    CloseHandle(hProcess);
    binary_path = buf.data();

#else
    binary_path = "/usr/bin/mock_app";
#endif

    if (monday_is_process_blocked(binary_path.c_str())) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_AUTOMATION_BLOCKED, "Process is blocklisted from automation"));
    }

    return binary_path;
}

} // namespace monday
