// tests/integration/test_plugin_lifecycle.cpp
//
// Integration tests for plugin execution lifecycle, IPC, capability checks, and crash restart.

#include <catch2/catch_test_macros.hpp>
#include "monday/plugin/plugin_host.h"
#include "monday/plugin/capability_broker.h"
#include "monday/plugin/lua_bridge.h"
#include "monday/core/errors.h"
#include <fstream>
#include <cstdio>
#include <chrono>
#include <thread>

using namespace monday;

TEST_CASE("PluginLifecycle: Host execution, capability broker and restart", "[plugin_integration]") {
    CapabilityBroker broker;
    PluginHost host(&broker);

    std::string script_path = "/tmp/test_lifecycle_script.lua";
    {
        std::ofstream file(script_path, std::ios::trunc);
        file << "local val, err = monday.read_blackboard('plugin.test_plugin.key')\n";
        file << "if not val then error(err) end\n";
    }

    PluginManifest manifest;
    manifest.plugin_id = "test_plugin";
    manifest.name = "Lifecycle Test Plugin";
    manifest.version = "1.0.0";
    manifest.script_path = script_path;
    manifest.capabilities.push_back("plugin.test_plugin.*");

    broker.register_plugin(manifest.plugin_id, manifest.capabilities);

    // 1. Authorized read blackboard
    auto run_res = host.run_script(manifest.plugin_id, script_path);
    REQUIRE(run_res.has_value());

    // 2. Unauthorized read blackboard -> MONDAY_ERR_PERMISSION_DENIED
    std::string unauthorized_script = "/tmp/unauthorized_script.lua";
    {
        std::ofstream file(unauthorized_script, std::ios::trunc);
        file << "local val, err = monday.read_blackboard('engram.episodic.secret')\n";
        file << "if err == 'MONDAY_ERR_PERMISSION_DENIED' then print('Permission denied as expected') else error('Access was not denied') end\n";
    }

    auto unauth_res = host.run_script(manifest.plugin_id, unauthorized_script);
    REQUIRE(unauth_res.has_value());

    // 3. Launch subprocess and test process restart tracking
    auto launch_res = host.launch_plugin(manifest);
    REQUIRE(launch_res.has_value());
    REQUIRE(host.get_active_plugin_count() == 1);

    // Handle process deaths / restart loop
    host.handle_process_deaths();
    REQUIRE(host.stop_plugin(manifest.plugin_id).has_value());

    std::remove(script_path.c_str());
    std::remove(unauthorized_script.c_str());
}
