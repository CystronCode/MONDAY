// trusted-helper/cherry_dialog.h
#pragma once

#include <string>

namespace monday {
namespace helper {

class CherryDialog {
public:
    CherryDialog() = default;
    ~CherryDialog() = default;

    std::string prompt_cherry(const std::string& operation_description);
};

} // namespace helper
} // namespace monday
