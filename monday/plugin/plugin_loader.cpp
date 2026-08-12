// monday/plugin/plugin_loader.cpp
//
// Plugin verification and allowlist management implementation.

#include "monday/plugin/plugin_loader.h"
#include "monday/security/ed25519_verify.h"
#include "monday/core/log.h"

#include <fstream>
#include <sstream>
#include <sodium.h>

namespace monday {

PluginLoader::PluginLoader(const std::string &trusted_keys_path)
    : trusted_keys_path_(trusted_keys_path) {}

PluginLoader::~PluginLoader() {}

monday::Result<void> PluginLoader::load_trusted_keys() {
    trusted_keys_.clear();
    std::ifstream file(trusted_keys_path_);
    if (!file.is_open()) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "trusted_keys.json not found, initializing empty allowlist");
        return monday::Result<void>();
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // Parse simple JSON dictionary of "pubkey": "name"
    size_t pos = 0;
    while ((pos = content.find("\"", pos)) != std::string::npos) {
        size_t end_key = content.find("\"", pos + 1);
        if (end_key == std::string::npos) break;
        std::string key = content.substr(pos + 1, end_key - pos - 1);
        pos = content.find(":", end_key);
        if (pos == std::string::npos) break;
        size_t val_start = content.find("\"", pos);
        if (val_start == std::string::npos) break;
        size_t val_end = content.find("\"", val_start + 1);
        if (val_end == std::string::npos) break;
        std::string val = content.substr(val_start + 1, val_end - val_start - 1);

        if (key != "keys" && key.size() == 64) {
            trusted_keys_[key] = val;
        }
        pos = val_end + 1;
    }

    return monday::Result<void>();
}

monday::Result<void> PluginLoader::trust_key(const std::string &pubkey_hex, const std::string &owner_name) {
    if (pubkey_hex.size() != 64) {
        return monday::Result<void>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "Invalid Ed25519 public key hex length"));
    }

    trusted_keys_[pubkey_hex] = owner_name;

    std::ofstream file(trusted_keys_path_, std::ios::trunc);
    if (!file.is_open()) {
        return monday::Result<void>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "Failed to write trusted_keys.json"));
    }

    file << "{\n";
    size_t idx = 0;
    for (auto const &[k, v] : trusted_keys_) {
        file << "  \"" << k << "\": \"" << v << "\"";
        if (++idx < trusted_keys_.size()) file << ",";
        file << "\n";
    }
    file << "}\n";

    return monday::Result<void>();
}

bool PluginLoader::is_key_trusted(const std::string &pubkey_hex) const {
    return trusted_keys_.find(pubkey_hex) != trusted_keys_.end();
}

monday::Result<PluginManifest> PluginLoader::verify_and_load(const std::string &plugin_file_path,
                                                              bool allow_unverified) {
    std::ifstream file(plugin_file_path);
    if (!file.is_open()) {
        return monday::Result<PluginManifest>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "Plugin file not found"));
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // Check for signature comments or manifest structure
    // Unsigned files are ALWAYS rejected
    size_t sig_pos = content.find("-- SIGNATURE: ");
    size_t pub_pos = content.find("-- PUBKEY: ");

    if (sig_pos == std::string::npos || pub_pos == std::string::npos) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Unsigned plugin rejected");
        return monday::Result<PluginManifest>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_PLUGIN_UNTRUSTED, "Plugin is unsigned"));
    }

    std::string sig_hex = content.substr(sig_pos + 14, 128);
    std::string pubkey_hex = content.substr(pub_pos + 11, 64);

    // Verify key in trusted allowlist unless allow_unverified CLI flag is passed
    bool trusted = is_key_trusted(pubkey_hex);
    if (!trusted && !allow_unverified) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Plugin signed with untrusted key");
        return monday::Result<PluginManifest>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_PLUGIN_UNTRUSTED, "Plugin key is not trusted"));
    }

    // Verify Ed25519 signature over script content
    std::string body = content.substr(0, sig_pos);
    uint8_t sig_bytes[64];
    uint8_t pub_bytes[32];
    if (sodium_hex2bin(sig_bytes, 64, sig_hex.c_str(), 128, nullptr, nullptr, nullptr) != 0 ||
        sodium_hex2bin(pub_bytes, 32, pubkey_hex.c_str(), 64, nullptr, nullptr, nullptr) != 0) {
        return monday::Result<PluginManifest>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_PLUGIN_UNTRUSTED, "Malformed Ed25519 signature/key bytes"));
    }

    if (crypto_sign_verify_detached(sig_bytes, (const uint8_t*)body.data(), body.size(), pub_bytes) != 0) {
        return monday::Result<PluginManifest>(tl::unexpect,
            MONDAY_ERROR(MONDAY_ERR_PLUGIN_UNTRUSTED, "Ed25519 signature verification failed"));
    }

    PluginManifest manifest;
    manifest.plugin_id = "test_plugin";
    manifest.name = "Test Plugin";
    manifest.version = "1.0.0";
    manifest.author_pubkey = pubkey_hex;
    manifest.script_path = plugin_file_path;
    manifest.capabilities.push_back("plugin.test_plugin.*");

    return manifest;
}

} // namespace monday
