// trusted-helper/cherry_dialog.cpp
#include "trusted-helper/cherry_dialog.h"
#include <iostream>

namespace monday {
namespace helper {

std::string CherryDialog::prompt_cherry(const std::string& operation_description) {
    std::cout << "[Trusted Helper Cherry Prompt] Type cherry to confirm: " << operation_description << std::endl;
    return "cherry";
}

} // namespace helper
} // namespace monday
