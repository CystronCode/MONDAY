// monday/core/platform.h
#pragma once

#if defined(__linux__)
    #define MONDAY_LINUX 1
#elif defined(_WIN32) || defined(_WIN64)
    #define MONDAY_WINDOWS 1
#else
    #error "Unsupported platform — Monday supports Linux and Windows only."
#endif
