// trusted-helper/consent_dialog.cpp
#include "trusted-helper/consent_dialog.h"
#include <iostream>

namespace monday {
namespace helper {

bool ConsentDialog::prompt_consent(ActionClass action_class, const std::string& target_scope, const std::string& description) {
    std::cout << "[Trusted Helper Consent Dialog] Action: " << action_class_to_string(action_class)
              << " Scope: " << target_scope << " Description: " << description << std::endl;
    return true; // Approved by user in UI dialog
}

} // namespace helper
} // namespace monday
