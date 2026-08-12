// tests/unit/test_plugin_loader.cpp
//
// Unit tests for plugin signature verification and trusted keys.

#include <catch2/catch_test_macros.hpp>
#include "monday/plugin/plugin_loader.h"
#include "monday/core/errors.h"
#include <sodium.h>
#include <fstream>
#include <cstdio>
#include <string>

using namespace monday;

TEST_CASE("PluginLoader: Signature and allowlist enforcement", "[plugin]") {
    REQUIRE(sodium_init() >= 0);

    std::string keys_file = "/tmp/test_trusted_keys.json";
    std::string plugin_file = "/tmp/test_plugin.lua";

    // Generate test Ed25519 keypair
    uint8_t pk[crypto_sign_PUBLICKEYBYTES];
    uint8_t sk[crypto_sign_SECRETKEYBYTES];
    REQUIRE(crypto_sign_keypair(pk, sk) == 0);

    char pk_hex[crypto_sign_PUBLICKEYBYTES * 2 + 1];
    sodium_bin2hex(pk_hex, sizeof(pk_hex), pk, sizeof(pk));

    // Write trusted_keys.json
    {
        std::ofstream fk(keys_file, std::ios::trunc);
        fk << "{\n  \"" << pk_hex << "\": \"trusted_dev\"\n}\n";
    }

    PluginLoader loader(keys_file);
    REQUIRE(loader.load_trusted_keys().has_value());
    REQUIRE(loader.is_key_trusted(pk_hex));

    // 1. Create a signed script
    std::string body = "-- Monday Plugin\nmonday.log('hello')\n";
    uint8_t sig[crypto_sign_BYTES];
    REQUIRE(crypto_sign_detached(sig, nullptr, (const uint8_t*)body.data(), body.size(), sk) == 0);

    char sig_hex[crypto_sign_BYTES * 2 + 1];
    sodium_bin2hex(sig_hex, sizeof(sig_hex), sig, sizeof(sig));

    {
        std::ofstream fp(plugin_file, std::ios::trunc);
        fp << body;
        fp << "-- SIGNATURE: " << sig_hex << "\n";
        fp << "-- PUBKEY: " << pk_hex << "\n";
    }

    // 2. Load signed & trusted plugin -> success
    auto res = loader.verify_and_load(plugin_file, false);
    REQUIRE(res.has_value());

    // 3. Test unsigned plugin -> MONDAY_ERR_PLUGIN_UNTRUSTED (always)
    std::string unsigned_file = "/tmp/unsigned_plugin.lua";
    {
        std::ofstream fu(unsigned_file, std::ios::trunc);
        fu << "monday.log('unsigned')\n";
    }

    auto res_uns = loader.verify_and_load(unsigned_file, true);
    REQUIRE_FALSE(res_uns.has_value());
    REQUIRE(res_uns.error().code == MONDAY_ERR_PLUGIN_UNTRUSTED);

    // 4. Test untrusted signed key
    uint8_t pk2[crypto_sign_PUBLICKEYBYTES];
    uint8_t sk2[crypto_sign_SECRETKEYBYTES];
    REQUIRE(crypto_sign_keypair(pk2, sk2) == 0);

    char pk2_hex[crypto_sign_PUBLICKEYBYTES * 2 + 1];
    sodium_bin2hex(pk2_hex, sizeof(pk2_hex), pk2, sizeof(pk2));

    uint8_t sig2[crypto_sign_BYTES];
    REQUIRE(crypto_sign_detached(sig2, nullptr, (const uint8_t*)body.data(), body.size(), sk2) == 0);

    char sig2_hex[crypto_sign_BYTES * 2 + 1];
    sodium_bin2hex(sig2_hex, sizeof(sig2_hex), sig2, sizeof(sig2));

    std::string untrusted_file = "/tmp/untrusted_plugin.lua";
    {
        std::ofstream fu(untrusted_file, std::ios::trunc);
        fu << body;
        fu << "-- SIGNATURE: " << sig2_hex << "\n";
        fu << "-- PUBKEY: " << pk2_hex << "\n";
    }

    // Untrusted key without flag -> MONDAY_ERR_PLUGIN_UNTRUSTED
    auto res_untr = loader.verify_and_load(untrusted_file, false);
    REQUIRE_FALSE(res_untr.has_value());
    REQUIRE(res_untr.error().code == MONDAY_ERR_PLUGIN_UNTRUSTED);

    // Untrusted key with allow_unverified flag -> success for session
    auto res_untr_allowed = loader.verify_and_load(untrusted_file, true);
    REQUIRE(res_untr_allowed.has_value());

    // 5. monday plugin trust flow -> add key and verify persistence
    REQUIRE(loader.trust_key(pk2_hex, "new_dev").has_value());
    REQUIRE(loader.is_key_trusted(pk2_hex));
    auto res_trust_now = loader.verify_and_load(untrusted_file, false);
    REQUIRE(res_trust_now.has_value());

    // Cleanup
    std::remove(keys_file.c_str());
    std::remove(plugin_file.c_str());
    std::remove(unsigned_file.c_str());
    std::remove(untrusted_file.c_str());
}
