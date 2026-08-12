// monday/plugin/capability_broker.h
//
// Per-call namespace capability enforcement (§3.4, Phase 5).

#pragma once

#include <string>
#include <vector>
#include <set>
#include "monday/core/result.h"
#include "monday/core/errors.h"

namespace monday {

class CapabilityBroker {
public:
    CapabilityBroker();
    ~CapabilityBroker();

    // Register a plugin's granted capabilities (e.g., "plugin.myplugin.*")
    void register_plugin(const std::string &plugin_id, const std::vector<std::string> &capabilities);

    // Unregister a plugin
    void unregister_plugin(const std::string &plugin_id);

    // Verify if plugin is allowed to access a Blackboard key namespace or system capability
    bool check_blackboard_access(const std::string &plugin_id,
                                 const std::string &key,
                                 bool is_write) const;

    bool check_capability(const std::string &plugin_id,
                          const std::string &required_cap) const;

private:
    std::set<std::pair<std::string, std::string>> plugin_capabilities_; // (plugin_id, cap_pattern)
};

} // namespace monday
