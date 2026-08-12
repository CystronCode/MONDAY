// monday/automation/linux/atspi_driver.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>
#include <vector>
#include <memory>

namespace monday {

struct AccessibleElement {
    std::string name;
    std::string role;
    int pid{0};
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    std::vector<std::string> actions;
};

class AtSpiDriver {
public:
    AtSpiDriver() = default;
    ~AtSpiDriver() = default;

    Result<void> initialize();
    Result<std::vector<AccessibleElement>> get_top_level_windows();
    Result<AccessibleElement> find_element(int pid, const std::string& name_or_role);
    Result<void> invoke_action(const AccessibleElement& elem, const std::string& action_name);
};

} // namespace monday
