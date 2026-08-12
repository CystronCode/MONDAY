// monday/plugin/plugin_host.h
//
// Plugin host process lifecycle manager (§3.4, Phase 5).

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <sys/types.h>
#include "monday/core/result.h"
#include "monday/core/errors.h"
#include "monday/plugin/plugin_loader.h"
#include "monday/plugin/capability_broker.h"

namespace monday {

struct PluginInstance {
    std::string plugin_id;
    std::string script_path;
    pid_t pid;
    bool active;
    uint32_t restart_count;
    std::chrono::steady_clock::time_point last_start_time;
};

class PluginHost {
public:
    PluginHost(CapabilityBroker *broker);
    ~PluginHost();

    // Start a plugin subprocess.
    monday::Result<pid_t> launch_plugin(const PluginManifest &manifest);

    // Stop a running plugin subprocess.
    monday::Result<void> stop_plugin(const std::string &plugin_id);

    // Run in-process Lua VM script (for lifecycle tests / host execution).
    monday::Result<void> run_script(const std::string &plugin_id, const std::string &script_path);

    // Check status of plugin processes and restart if dead (call on SIGCHLD or timer).
    void handle_process_deaths();

    // Retrieve active instance count
    size_t get_active_plugin_count() const;

private:
    CapabilityBroker *broker_;
    std::map<std::string, PluginInstance> instances_;
};

} // namespace monday
