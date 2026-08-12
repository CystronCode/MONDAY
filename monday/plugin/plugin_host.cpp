// monday/plugin/plugin_host.cpp
//
// Plugin host process lifecycle implementation.

#include "monday/plugin/plugin_host.h"
#include "monday/plugin/lua_bridge.h"
#include "monday/security/seccomp_filter.h"
#include "monday/core/log.h"

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <lua.hpp>
#include <cstring>
#include <iostream>

namespace monday {

PluginHost::PluginHost(CapabilityBroker *broker) : broker_(broker) {}

PluginHost::~PluginHost() {
    for (auto &[id, inst] : instances_) {
        if (inst.active && inst.pid > 0) {
            kill(inst.pid, SIGTERM);
            waitpid(inst.pid, nullptr, WNOHANG);
        }
    }
}

monday::Result<pid_t> PluginHost::launch_plugin(const PluginManifest &manifest) {
    if (broker_) {
        broker_->register_plugin(manifest.plugin_id, manifest.capabilities);
    }

    pid_t pid = fork();
    if (pid < 0) {
        return monday::Result<pid_t>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_IPC_PEER_DIED, "Fork failed for plugin process"));
    }

    if (pid == 0) {
        // Child process execution sandbox
#ifdef MONDAY_LINUX
        monday_seccomp_install_filter();
#endif

        lua_State *L = luaL_newstate();
        if (L) {
            luaL_openlibs(L);
            LuaBridge::register_bridge(L, manifest.plugin_id, broker_);

            if (luaL_dofile(L, manifest.script_path.c_str()) != LUA_OK) {
                const char *err = lua_tostring(L, -1);
                std::cerr << "Lua plugin error: " << (err ? err : "unknown") << std::endl;
            }
            lua_close(L);
        }
        _exit(0);
    }

    // Parent process
    PluginInstance inst;
    inst.plugin_id = manifest.plugin_id;
    inst.script_path = manifest.script_path;
    inst.pid = pid;
    inst.active = true;
    inst.restart_count = 0;
    inst.last_start_time = std::chrono::steady_clock::now();

    instances_[manifest.plugin_id] = inst;

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Launched plugin process");
    return pid;
}

monday::Result<void> PluginHost::stop_plugin(const std::string &plugin_id) {
    auto it = instances_.find(plugin_id);
    if (it == instances_.end() || !it->second.active) {
        return monday::Result<void>();
    }

    if (it->second.pid > 0) {
        kill(it->second.pid, SIGTERM);
        waitpid(it->second.pid, nullptr, 0);
    }

    it->second.active = false;
    if (broker_) {
        broker_->unregister_plugin(plugin_id);
    }

    return monday::Result<void>();
}

monday::Result<void> PluginHost::run_script(const std::string &plugin_id, const std::string &script_path) {
    lua_State *L = luaL_newstate();
    if (!L) {
        return monday::Result<void>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "Failed to create Lua state"));
    }

    luaL_openlibs(L);
    LuaBridge::register_bridge(L, plugin_id, broker_);

    if (luaL_dofile(L, script_path.c_str()) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        std::string err_msg = err ? err : "Lua execution failed";
        lua_close(L);
        return monday::Result<void>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, err_msg.c_str()));
    }

    lua_close(L);
    return monday::Result<void>();
}

void PluginHost::handle_process_deaths() {
    int status;
    pid_t died_pid;
    while ((died_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (auto &[id, inst] : instances_) {
            if (inst.pid == died_pid && inst.active) {
                monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Plugin process died, restarting within 2s");
                inst.restart_count++;

                // Re-launch child process
                pid_t new_pid = fork();
                if (new_pid == 0) {
#ifdef MONDAY_LINUX
                    monday_seccomp_install_filter();
#endif
                    lua_State *L = luaL_newstate();
                    if (L) {
                        luaL_openlibs(L);
                        LuaBridge::register_bridge(L, inst.plugin_id, broker_);
                        luaL_dofile(L, inst.script_path.c_str());
                        lua_close(L);
                    }
                    _exit(0);
                } else if (new_pid > 0) {
                    inst.pid = new_pid;
                    inst.last_start_time = std::chrono::steady_clock::now();
                }
                break;
            }
        }
    }
}

size_t PluginHost::get_active_plugin_count() const {
    size_t count = 0;
    for (auto const &[id, inst] : instances_) {
        if (inst.active) count++;
    }
    return count;
}

} // namespace monday
