// trusted-helper/consent_dialog.h
#pragma once

#include "monday/security/consent.h"
#include <string>

namespace monday {
namespace helper {

class ConsentDialog {
public:
    ConsentDialog() = default;
    ~ConsentDialog() = default;

    bool prompt_consent(ActionClass action_class, const std::string& target_scope, const std::string& description);
};

} // namespace helper
} // namespace monday
