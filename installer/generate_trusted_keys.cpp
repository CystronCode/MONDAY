// installer/generate_trusted_keys.cpp
//
// First-run developer keypair generator (§3.4, Phase 5).

#include <sodium.h>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium" << std::endl;
        return 1;
    }

    std::string output_path = "trusted_keys.json";
    if (argc > 1) {
        output_path = argv[1];
    }

    uint8_t pk[crypto_sign_PUBLICKEYBYTES];
    uint8_t sk[crypto_sign_SECRETKEYBYTES];

    if (crypto_sign_keypair(pk, sk) != 0) {
        std::cerr << "Failed to generate Ed25519 keypair" << std::endl;
        return 1;
    }

    char pk_hex[crypto_sign_PUBLICKEYBYTES * 2 + 1];
    sodium_bin2hex(pk_hex, sizeof(pk_hex), pk, sizeof(pk));

    std::ofstream file(output_path, std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << output_path << std::endl;
        return 1;
    }

    file << "{\n";
    file << "  \"" << pk_hex << "\": \"local_developer\"\n";
    file << "}\n";

    std::cout << "Successfully generated developer key pair and written to " << output_path << std::endl;
    return 0;
}
