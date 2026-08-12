// monday/plugin/plugin_loader.h
//
// Plugin signature verification and trusted key allowlist management (§3.4, Phase 5).

#pragma once

#include <string>
#include <vector>
#include <map>
#include "monday/core/result.h"
#include "monday/core/errors.h"

namespace monday {

struct PluginManifest {
    std::string plugin_id;
    std::string name;
    std::string version;
    std::string author_pubkey;
    std::vector<std::string> capabilities;
    std::string script_path;
};

class PluginLoader {
public:
    PluginLoader(const std::string &trusted_keys_path);
    ~PluginLoader();

    // Load trusted_keys.json into memory.
    monday::Result<void> load_trusted_keys();

    // Add a public key (hex encoded 32 bytes) to trusted_keys.json.
    monday::Result<void> trust_key(const std::string &pubkey_hex, const std::string &owner_name);

    // Check if public key is present in trusted allowlist.
    bool is_key_trusted(const std::string &pubkey_hex) const;

    // Verify and load plugin.
    // allow_unverified: true if --allow-unverified-plugin was passed on CLI.
    monday::Result<PluginManifest> verify_and_load(const std::string &plugin_file_path,
                                                   bool allow_unverified);

private:
    std::string trusted_keys_path_;
    std::map<std::string, std::string> trusted_keys_; // pubkey_hex -> owner_name
};

} // namespace monday
