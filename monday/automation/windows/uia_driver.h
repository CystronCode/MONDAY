// monday/automation/windows/uia_driver.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>
#include <vector>

namespace monday {

struct WinUIElement {
    std::string name;
    std::string control_type;
    int pid{0};
    std::vector<std::string> supported_patterns;
};

class UIADriver {
public:
    UIADriver() = default;
    ~UIADriver() = default;

    Result<void> initialize();
    Result<std::vector<WinUIElement>> get_top_level_windows();
    Result<WinUIElement> find_element(int pid, const std::string& name_or_type);
    Result<void> invoke_pattern(const WinUIElement& elem, const std::string& pattern_name);
};

} // namespace monday
