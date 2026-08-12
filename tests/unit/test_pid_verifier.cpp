// tests/unit/test_pid_verifier.cpp
#include <catch2/catch_test_macros.hpp>
#include "monday/automation/pid_verifier.h"
#include "monday/automation/automation_blocklist.h"

#if defined(__linux__)
#include <unistd.h>
#endif

TEST_CASE("PID Verifier validation tests", "[pid_verifier]") {
    SECTION("PID 0 returns MONDAY_ERR_INVALID_ARG") {
        auto res = monday::verify_pid_binary_path(0);
        REQUIRE(!res.has_value());
        REQUIRE(res.error().code == MONDAY_ERR_INVALID_ARG);
    }

    SECTION("Negative PID returns MONDAY_ERR_INVALID_ARG") {
        auto res = monday::verify_pid_binary_path(-10);
        REQUIRE(!res.has_value());
        REQUIRE(res.error().code == MONDAY_ERR_INVALID_ARG);
    }

    SECTION("Non-existent PID returns MONDAY_ERR_NOT_FOUND") {
        auto res = monday::verify_pid_binary_path(999999);
        REQUIRE(!res.has_value());
        REQUIRE(res.error().code == MONDAY_ERR_NOT_FOUND);
    }

    SECTION("Current process PID resolution succeeds") {
#if defined(__linux__)
        int my_pid = static_cast<int>(getpid());
        auto res = monday::verify_pid_binary_path(my_pid);
        REQUIRE(res.has_value());
        REQUIRE(!res.value().empty());
#endif
    }

    SECTION("Automation blocklist string matching") {
        REQUIRE(monday_is_process_blocked("/usr/bin/sudo") == true);
        REQUIRE(monday_is_process_blocked("/usr/bin/polkit-agent-helper-1") == true);
        REQUIRE(monday_is_process_blocked("/usr/bin/keepassxc") == true);
        REQUIRE(monday_is_process_blocked("C:\\Windows\\System32\\consent.exe") == true);
        REQUIRE(monday_is_process_blocked("/usr/bin/ls") == false);
        REQUIRE(monday_is_process_blocked("/usr/bin/gedit") == false);
    }
}
