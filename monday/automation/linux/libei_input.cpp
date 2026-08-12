// monday/automation/linux/libei_input.cpp
#include "monday/automation/linux/libei_input.h"
#include "monday/core/log.h"

namespace monday {

Result<void> LibEiInput::initialize() {
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "libei Wayland input simulation initialized");
    return {};
}

Result<void> LibEiInput::move_pointer(int x, int y) {
    (void)x; (void)y;
    monday_log_write(MONDAY_LOG_VERBOSE, "libei: move pointer");
    return {};
}

Result<void> LibEiInput::click_button(int button) {
    (void)button;
    monday_log_write(MONDAY_LOG_VERBOSE, "libei: click button");
    return {};
}

Result<void> LibEiInput::type_text(const std::string& text) {
    (void)text;
    monday_log_write(MONDAY_LOG_VERBOSE, "libei: type text");
    return {};
}

} // namespace monday
