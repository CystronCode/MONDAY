// monday/plugin/capability_broker.cpp
//
// Capability broker implementation.

#include "monday/plugin/capability_broker.h"
#include "monday/core/log.h"
#include <algorithm>

namespace monday {

CapabilityBroker::CapabilityBroker() {}

CapabilityBroker::~CapabilityBroker() {}

void CapabilityBroker::register_plugin(const std::string &plugin_id, const std::vector<std::string> &capabilities) {
    for (const auto &cap : capabilities) {
        plugin_capabilities_.insert({plugin_id, cap});
    }
}

void CapabilityBroker::unregister_plugin(const std::string &plugin_id) {
    for (auto it = plugin_capabilities_.begin(); it != plugin_capabilities_.end();) {
        if (it->first == plugin_id) {
            it = plugin_capabilities_.erase(it);
        } else {
            ++it;
        }
    }
}

bool CapabilityBroker::check_blackboard_access(const std::string &plugin_id,
                                                const std::string &key,
                                                bool is_write) const {
    (void)is_write;
    // Plugins can only access keys in their own plugin namespace (e.g. plugin.id.*)
    // unless explicitly granted a capability.
    std::string allowed_prefix = "plugin." + plugin_id + ".";
    if (key.rfind(allowed_prefix, 0) == 0) {
        return true;
    }

    // Check if plugin holds explicit capability wildcard matching key
    for (const auto &entry : plugin_capabilities_) {
        if (entry.first == plugin_id) {
            const std::string &cap = entry.second;
            if (cap == "*" || cap == key) {
                return true;
            }
            if (!cap.empty() && cap.back() == '*') {
                std::string prefix = cap.substr(0, cap.size() - 1);
                if (key.rfind(prefix, 0) == 0) {
                    return true;
                }
            }
        }
    }

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Capability check failed for plugin access to key");
    return false;
}

bool CapabilityBroker::check_capability(const std::string &plugin_id,
                                         const std::string &required_cap) const {
    return plugin_capabilities_.find({plugin_id, required_cap}) != plugin_capabilities_.end() ||
           plugin_capabilities_.find({plugin_id, "*"}) != plugin_capabilities_.end();
}

} // namespace monday
