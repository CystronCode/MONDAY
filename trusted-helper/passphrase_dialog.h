// trusted-helper/passphrase_dialog.h
#pragma once

#include <string>

namespace monday {
namespace helper {

class PassphraseDialog {
public:
    PassphraseDialog() = default;
    ~PassphraseDialog() = default;

    std::string prompt_passphrase(const std::string& prompt_title);
};

} // namespace helper
} // namespace monday
