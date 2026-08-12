// monday/automation/linux/libxdo_input.cpp
#include "monday/automation/linux/libxdo_input.h"
#include "monday/core/log.h"

namespace monday {

Result<void> LibXdoInput::initialize() {
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "libxdo X11 input simulation initialized");
    return {};
}

Result<void> LibXdoInput::move_pointer(int x, int y) {
    (void)x; (void)y;
    monday_log_write(MONDAY_LOG_VERBOSE, "libxdo: move pointer");
    return {};
}

Result<void> LibXdoInput::click_button(int button) {
    (void)button;
    monday_log_write(MONDAY_LOG_VERBOSE, "libxdo: click button");
    return {};
}

Result<void> LibXdoInput::type_text(const std::string& text) {
    (void)text;
    monday_log_write(MONDAY_LOG_VERBOSE, "libxdo: type text");
    return {};
}

} // namespace monday
