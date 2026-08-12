// monday/automation/windows/win32_input.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>

namespace monday {

class Win32Input {
public:
    Win32Input() = default;
    ~Win32Input() = default;

    Result<void> send_mouse_click(int x, int y) {
        return {};
    }

    Result<void> send_keystrokes(const std::string& text) {
        return {};
    }
};

} // namespace monday
