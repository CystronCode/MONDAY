// monday/automation/linux/libei_input.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>

namespace monday {

class LibEiInput {
public:
    LibEiInput() = default;
    ~LibEiInput() = default;

    Result<void> initialize();
    Result<void> move_pointer(int x, int y);
    Result<void> click_button(int button);
    Result<void> type_text(const std::string& text);
};

} // namespace monday
