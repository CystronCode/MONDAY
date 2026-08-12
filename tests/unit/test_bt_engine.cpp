// tests/unit/test_bt_engine.cpp
//
// Unit tests for Behavior Tree engine.

#include <catch2/catch_test_macros.hpp>
#include "monday/bt/bt_engine.h"
#include "monday/core/allocator.h"
#include <chrono>

using namespace monday;

TEST_CASE("BTEngine: Sequence and Selector node logic", "[bt]") {
    monday_allocator_init(4*1024*1024, 8*1024*1024, 4*1024*1024, 8*1024*1024);

    auto seq = std::make_shared<SequenceNode>();
    auto cond1 = std::make_shared<ConditionNode>("system_status", "ready");
    auto cond2 = std::make_shared<ConditionNode>("network_status", "offline");

    seq->add_child(cond1);
    seq->add_child(cond2);

    BTEngine engine;
    engine.set_root(seq);

    auto start = std::chrono::high_resolution_clock::now();
    NodeStatus status = engine.tick();
    auto elapsed = std::chrono::high_resolution_clock::now() - start;

    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    INFO("BT tick latency: " << elapsed_us << " us");
    REQUIRE(elapsed_us < 1000);
    REQUIRE(status == NodeStatus::SUCCESS);
}
