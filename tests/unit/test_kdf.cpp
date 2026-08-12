// tests/unit/test_kdf.cpp
//
// Unit tests for KDF (Argon2id).

#include <catch2/catch_test_macros.hpp>
#include "monday/security/kdf.h"
#include <string.h>
#include <chrono>

TEST_CASE("KDF: key derivation correctness and timing", "[security][kdf]") {
    uint8_t salt1[32] = {1};
    uint8_t key1[32] = {0};
    uint8_t key2[32] = {0};

    // Same passphrase and salt must produce deterministic keys
    auto start = std::chrono::high_resolution_clock::now();
    monday_error_t err1 = monday_kdf_derive_key("password123", salt1, key1);
    auto end = std::chrono::high_resolution_clock::now();
    
    REQUIRE(err1.code == MONDAY_OK);
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    INFO("Argon2id Derivation latency: " << duration_ms << " ms");
    // Confirm security timing requirement: time cost should be significant
    REQUIRE(duration_ms > 10); 

    monday_error_t err2 = monday_kdf_derive_key("password123", salt1, key2);
    REQUIRE(err2.code == MONDAY_OK);
    REQUIRE(memcmp(key1, key2, 32) == 0);

    // Same passphrase, different salt -> different key
    uint8_t salt2[32] = {2};
    uint8_t key3[32] = {0};
    monday_error_t err3 = monday_kdf_derive_key("password123", salt2, key3);
    REQUIRE(err3.code == MONDAY_OK);
    REQUIRE(memcmp(key1, key3, 32) != 0);

    // Compute Hamming distance (expecting high entropy/diffusion, average 128 bits diff)
    int hamming_dist = 0;
    for (int i = 0; i < 32; i++) {
        uint8_t diff = key1[i] ^ key3[i];
        while (diff) {
            hamming_dist += (diff & 1);
            diff >>= 1;
        }
    }
    INFO("Hamming distance between keys: " << hamming_dist << " bits");
    REQUIRE(hamming_dist >= 100); 
}
