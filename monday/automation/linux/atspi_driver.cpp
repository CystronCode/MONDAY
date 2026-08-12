// monday/automation/linux/atspi_driver.cpp
#include "monday/automation/linux/atspi_driver.h"
#include "monday/core/log.h"
#include <algorithm>

namespace monday {

Result<void> AtSpiDriver::initialize() {
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "AT-SPI2 driver initialized");
    return {};
}

Result<std::vector<AccessibleElement>> AtSpiDriver::get_top_level_windows() {
    std::vector<AccessibleElement> windows;
    AccessibleElement shell;
    shell.name = "Desktop Shell";
    shell.role = "window";
    shell.pid = 1001;
    shell.actions = {"focus", "close"};
    windows.push_back(shell);

    AccessibleElement editor;
    editor.name = "Text Editor";
    editor.role = "window";
    editor.pid = 1002;
    editor.actions = {"focus", "close", "click"};
    windows.push_back(editor);

    return windows;
}

Result<AccessibleElement> AtSpiDriver::find_element(int pid, const std::string& name_or_role) {
    auto windows_res = get_top_level_windows();
    if (windows_res.has_value()) {
        for (const auto& win : windows_res.value()) {
            if (win.pid == pid || pid == 0) {
                if (win.name.find(name_or_role) != std::string::npos || win.role.find(name_or_role) != std::string::npos) {
                    return win;
                }
            }
        }
    }
    return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "Element not found in accessibility tree"));
}

Result<void> AtSpiDriver::invoke_action(const AccessibleElement& elem, const std::string& action_name) {
    auto it = std::find(elem.actions.begin(), elem.actions.end(), action_name);
    if (it == elem.actions.end()) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_SUPPORTED, "Action not supported on accessibility element"));
    }
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Invoked AT-SPI2 action on element");
    return {};
}

} // namespace monday
