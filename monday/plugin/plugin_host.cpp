// monday/plugin/plugin_host.cpp
//
// Plugin host process lifecycle implementation.

#include "monday/plugin/plugin_host.h"
#include "monday/plugin/lua_bridge.h"
#include "monday/security/seccomp_filter.h"
#include "monday/core/platform.h"
#include "monday/core/log.h"

#ifdef MONDAY_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include <lua.hpp>
#include <cstring>
#include <iostream>

namespace monday {

PluginHost::PluginHost(CapabilityBroker *broker) : broker_(broker) {}

PluginHost::~PluginHost() {
    for (auto &[id, inst] : instances_) {
        if (inst.active && inst.pid > 0) {
#ifndef MONDAY_WINDOWS
            kill(inst.pid, SIGKILL);
            waitpid(inst.pid, nullptr, WNOHANG);
#endif
        }
    }
}

static void reset_child_signals() {
#ifndef MONDAY_WINDOWS
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGSEGV, &sa, nullptr);
#endif
}

monday::Result<pid_t> PluginHost::launch_plugin(const PluginManifest &manifest) {
    if (broker_) {
        broker_->register_plugin(manifest.plugin_id, manifest.capabilities);
    }

#ifdef MONDAY_WINDOWS
    pid_t pid = 1000 + (pid_t)instances_.size();
    PluginInstance inst;
    inst.plugin_id = manifest.plugin_id;
    inst.script_path = manifest.script_path;
    inst.pid = pid;
    inst.active = true;
    inst.restart_count = 0;
    inst.last_start_time = std::chrono::steady_clock::now();

    instances_[manifest.plugin_id] = inst;
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Launched plugin in Windows host process");
    return pid;
#else
    pid_t pid = fork();
    if (pid < 0) {
        return monday::Result<pid_t>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_IPC_PEER_DIED, "Fork failed for plugin process"));
    }

    if (pid == 0) {
        reset_child_signals();
#ifdef MONDAY_LINUX
        monday_install_seccomp_filter();
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
#endif
}

monday::Result<void> PluginHost::stop_plugin(const std::string &plugin_id) {
    auto it = instances_.find(plugin_id);
    if (it == instances_.end() || !it->second.active) {
        return monday::Result<void>();
    }

    if (it->second.pid > 0) {
#ifndef MONDAY_WINDOWS
        kill(it->second.pid, SIGKILL);
        waitpid(it->second.pid, nullptr, 0);
#endif
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
#ifndef MONDAY_WINDOWS
    int status;
    pid_t died_pid;
    while ((died_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (auto &[id, inst] : instances_) {
            if (inst.pid == died_pid && inst.active) {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    inst.active = false;
                    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Plugin process exited normally");
                    break;
                }

                monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Plugin process crashed, restarting within 2s");
                inst.restart_count++;

                pid_t new_pid = fork();
                if (new_pid == 0) {
                    reset_child_signals();
#ifdef MONDAY_LINUX
                    monday_install_seccomp_filter();
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
#endif
}

size_t PluginHost::get_active_plugin_count() const {
    size_t count = 0;
    for (const auto &[id, inst] : instances_) {
        if (inst.active) count++;
    }
    return count;
}

} // namespace monday
