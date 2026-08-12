// tests/unit/test_consent.cpp
#include <catch2/catch_test_macros.hpp>
#include "monday/security/consent.h"
#include <sqlite3.h>

TEST_CASE("ConsentManager READ_ONLY action class", "[consent]") {
    monday::ConsentManager mgr;
    REQUIRE(mgr.init(nullptr, "test_session").has_value());

    int ui_call_count = 0;
    mgr.set_ui_handler([&](monday::ActionClass, const std::string&, const std::string&) {
        ui_call_count++;
        return true;
    });

    auto res = mgr.request_consent(monday::ActionClass::READ_ONLY, "/tmp", "Read file");
    REQUIRE(res.has_value());
    REQUIRE(res.value() == true);
    REQUIRE(ui_call_count == 0); // No UI dialog shown for READ_ONLY
}

TEST_CASE("ConsentManager WRITE permission grant lifecycle", "[consent]") {
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(":memory:", &db) == SQLITE_OK);

    monday::ConsentManager mgr;
    REQUIRE(mgr.init(db, "test_session").has_value());

    int ui_call_count = 0;
    mgr.set_ui_handler([&](monday::ActionClass, const std::string&, const std::string&) {
        ui_call_count++;
        return true;
    });

    // 1. First WRITE request: prompts UI, stores in DB
    auto res1 = mgr.request_consent(monday::ActionClass::WRITE, "/home/user/Documents", "Write file");
    REQUIRE(res1.has_value());
    REQUIRE(res1.value() == true);
    REQUIRE(ui_call_count == 1);

    // 2. Second WRITE request (same scope, active TTL): uses DB grant, UI NOT prompted
    auto res2 = mgr.request_consent(monday::ActionClass::WRITE, "/home/user/Documents", "Write another file");
    REQUIRE(res2.has_value());
    REQUIRE(res2.value() == true);
    REQUIRE(ui_call_count == 1); // Still 1! UI not called.

    // 3. Purging active grants (now) should not delete grant
    REQUIRE(mgr.purge_expired_grants().has_value());
    auto res3 = mgr.request_consent(monday::ActionClass::WRITE, "/home/user/Documents", "Write file 3");
    REQUIRE(res3.has_value());
    REQUIRE(res3.value() == true);
    REQUIRE(ui_call_count == 1);

    sqlite3_close(db);
}

TEST_CASE("ConsentManager DESTRUCTIVE action is never saved to DB", "[consent]") {
    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(":memory:", &db) == SQLITE_OK);

    monday::ConsentManager mgr;
    REQUIRE(mgr.init(db, "test_session").has_value());

    int ui_call_count = 0;
    mgr.set_ui_handler([&](monday::ActionClass, const std::string&, const std::string&) {
        ui_call_count++;
        return true;
    });

    // First DESTRUCTIVE request
    auto res1 = mgr.request_consent(monday::ActionClass::DESTRUCTIVE, "/home/user/data", "Delete directory");
    REQUIRE(res1.has_value());
    REQUIRE(res1.value() == true);
    REQUIRE(ui_call_count == 1);

    // Second DESTRUCTIVE request MUST prompt UI again (not saved to DB!)
    auto res2 = mgr.request_consent(monday::ActionClass::DESTRUCTIVE, "/home/user/data", "Delete directory");
    REQUIRE(res2.has_value());
    REQUIRE(res2.value() == true);
    REQUIRE(ui_call_count == 2); // Prompted twice!

    sqlite3_close(db);
}

TEST_CASE("ConsentManager Cherry typed confirmation validation", "[consent]") {
    REQUIRE(monday::ConsentManager::verify_cherry_input("cherry") == true);
    REQUIRE(monday::ConsentManager::verify_cherry_input("CHERRY") == true);
    REQUIRE(monday::ConsentManager::verify_cherry_input("  Cherry  ") == true);

    REQUIRE(monday::ConsentManager::verify_cherry_input("cherr") == false);
    REQUIRE(monday::ConsentManager::verify_cherry_input("cherry cherry") == false);
    REQUIRE(monday::ConsentManager::verify_cherry_input("yes") == false);

    monday::ConsentManager mgr;
    REQUIRE(mgr.init(nullptr, "test_session").has_value());

    // Valid cherry input
    auto res_valid = mgr.request_consent(monday::ActionClass::DESTRUCTIVE, "memory", "Clear memory", true, "cherry");
    REQUIRE(res_valid.has_value());
    REQUIRE(res_valid.value() == true);

    // Invalid cherry input
    auto res_invalid = mgr.request_consent(monday::ActionClass::DESTRUCTIVE, "memory", "Clear memory", true, "cherr");
    REQUIRE(!res_invalid.has_value());
    REQUIRE(res_invalid.error().code == MONDAY_ERR_CONSENT_DENIED);
}
