// tests/unit/test_goap.cpp
//
// Unit tests for GOAP Fast-Path Classifier (< 5 ms execution target).

#include <catch2/catch_test_macros.hpp>
#include "monday/goap/goap_planner.h"
#include "monday/core/allocator.h"
#include <chrono>

using namespace monday;

TEST_CASE("GOAPPlanner: timer intent classification", "[goap]") {
    monday_allocator_init(4*1024*1024, 8*1024*1024, 4*1024*1024, 8*1024*1024);

    GOAPPlanner planner;
    auto res_init = planner.init("data/intents.jsonl");
    REQUIRE(res_init.has_value());

    auto start = std::chrono::high_resolution_clock::now();
    GOAPResult res = planner.classify("set a timer for 10 minutes");
    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    INFO("GOAP classification latency: " << elapsed_ms << " ms");
    REQUIRE(elapsed_ms < 5);

    REQUIRE(res.matched == true);
    REQUIRE(res.confidence >= 0.85f);
    REQUIRE(res.action == "SYSTEM_SET_TIMER");
    REQUIRE(res.param_int_val == 600);
}

TEST_CASE("GOAPPlanner: destructive file delete threshold guard", "[goap]") {
    monday_allocator_init(4*1024*1024, 8*1024*1024, 4*1024*1024, 8*1024*1024);

    GOAPPlanner planner;
    REQUIRE(planner.init("data/intents.jsonl").has_value());

    GOAPResult res = planner.classify("delete this file");
    if (res.matched) {
        REQUIRE(res.confidence >= 0.95f);
        REQUIRE(res.action_class == ActionClass::DESTRUCTIVE);
    }
}
