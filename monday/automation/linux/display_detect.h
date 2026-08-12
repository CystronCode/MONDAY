// monday/automation/linux/display_detect.h
#pragma once

#include <cstdlib>
#include <string>

namespace monday {

enum class DisplayServerType {
    WAYLAND,
    X11,
    UNKNOWN
};

inline DisplayServerType detect_display_server() {
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    if (wayland && wayland[0] != '\0') {
        return DisplayServerType::WAYLAND;
    }
    const char* x11 = std::getenv("DISPLAY");
    if (x11 && x11[0] != '\0') {
        return DisplayServerType::X11;
    }
    return DisplayServerType::UNKNOWN;
}

} // namespace monday
