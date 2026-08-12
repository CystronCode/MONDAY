// monday/plugin/lua_bridge.h
//
// Lua C API bridge for Monday plugin bindings (§3.4, Phase 5).

#pragma once

#include <lua.hpp>
#include <string>
#include "monday/core/result.h"
#include "monday/core/errors.h"
#include "monday/plugin/capability_broker.h"

namespace monday {

class LuaBridge {
public:
    // Register monday.* functions into lua_State
    static void register_bridge(lua_State *L, const std::string &plugin_id, CapabilityBroker *broker);

    // C callbacks for lua
    static int lua_read_blackboard(lua_State *L);
    static int lua_write_blackboard(lua_State *L);
    static int lua_log(lua_State *L);
};

} // namespace monday
