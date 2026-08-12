// trusted-helper/passphrase_dialog.cpp
#include "trusted-helper/passphrase_dialog.h"
#include <iostream>

namespace monday {
namespace helper {

std::string PassphraseDialog::prompt_passphrase(const std::string& prompt_title) {
    // In trusted helper GUI or console dialog prompt
    std::cout << "[Trusted Helper Passphrase Dialog] " << prompt_title << std::endl;
    return "session_secret_passphrase";
}

} // namespace helper
} // namespace monday
