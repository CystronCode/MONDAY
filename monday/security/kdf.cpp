// monday/security/kdf.cpp
//
// Key Derivation Function (KDF) using Argon2id via libsodium.

#include "monday/security/kdf.h"
#include <sodium.h>
#include <string.h>

extern "C" {

monday_error_t monday_kdf_derive_key(const char *passphrase,
                                     const uint8_t salt[MONDAY_KDF_SALT_LEN],
                                     uint8_t out_key[MONDAY_KDF_KEY_LEN]) {
    if (!passphrase || !salt || !out_key) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null argument in key derivation");
    }

    // Initialize libsodium (safe to call multiple times)
    if (sodium_init() < 0) {
        return MONDAY_ERROR(MONDAY_ERR_INFERENCE_FAILED, "failed to initialize libsodium");
    }

    size_t passwd_len = strlen(passphrase);
    
    // Call crypto_pwhash representing Argon2id
    int res = crypto_pwhash(
        out_key, MONDAY_KDF_KEY_LEN,
        passphrase, passwd_len,
        salt,
        MONDAY_ARGON2_TIME_COST,
        MONDAY_ARGON2_MEM_LIMIT,
        crypto_pwhash_ALG_ARGON2ID13
    );

    if (res != 0) {
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "argon2id key derivation failed");
    }

    return MONDAY_ERROR(MONDAY_OK, "");
}

} // extern "C"
