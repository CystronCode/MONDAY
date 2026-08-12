// monday/security/kdf.h
//
// Key Derivation Function (KDF) using Argon2id.
// Architecture §3.7: Derives a 32-byte key from a passphrase and a salt.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MONDAY_KDF_KEY_LEN     32
#define MONDAY_KDF_SALT_LEN    32
#define MONDAY_ARGON2_TIME_COST 3
#define MONDAY_ARGON2_MEM_LIMIT (65536 * 1024) // 64 MB in bytes

// Derive a 32-byte key from a passphrase and a 32-byte salt using Argon2id.
// The password memory is zeroed out immediately after derivation.
monday_error_t monday_kdf_derive_key(const char *passphrase,
                                     const uint8_t salt[MONDAY_KDF_SALT_LEN],
                                     uint8_t out_key[MONDAY_KDF_KEY_LEN]);

#ifdef __cplusplus
}
#endif
