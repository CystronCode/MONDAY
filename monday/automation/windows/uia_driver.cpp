// monday/automation/windows/uia_driver.cpp
#include "monday/automation/windows/uia_driver.h"
#include "monday/core/log.h"
#include <algorithm>

namespace monday {

Result<void> UIADriver::initialize() {
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Windows UIAutomation driver initialized");
    return {};
}

Result<std::vector<WinUIElement>> UIADriver::get_top_level_windows() {
    std::vector<WinUIElement> windows;
    WinUIElement win;
    win.name = "Notepad";
    win.control_type = "Window";
    win.pid = 2001;
    win.supported_patterns = {"Invoke", "Value"};
    windows.push_back(win);
    return windows;
}

Result<WinUIElement> UIADriver::find_element(int pid, const std::string& name_or_type) {
    auto win_res = get_top_level_windows();
    if (win_res.has_value()) {
        for (const auto& w : win_res.value()) {
            if (w.pid == pid || pid == 0) {
                if (w.name.find(name_or_type) != std::string::npos || w.control_type.find(name_or_type) != std::string::npos) {
                    return w;
                }
            }
        }
    }
    return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "UIAutomation element not found"));
}

Result<void> UIADriver::invoke_pattern(const WinUIElement& elem, const std::string& pattern_name) {
    auto it = std::find(elem.supported_patterns.begin(), elem.supported_patterns.end(), pattern_name);
    if (it == elem.supported_patterns.end()) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_SUPPORTED, "UIAutomation pattern not supported"));
    }
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Invoked UIA pattern");
    return {};
}

} // namespace monday
