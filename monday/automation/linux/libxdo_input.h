// monday/automation/linux/libxdo_input.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>

namespace monday {

class LibXdoInput {
public:
    LibXdoInput() = default;
    ~LibXdoInput() = default;

    Result<void> initialize();
    Result<void> move_pointer(int x, int y);
    Result<void> click_button(int button);
    Result<void> type_text(const std::string& text);
};

} // namespace monday
