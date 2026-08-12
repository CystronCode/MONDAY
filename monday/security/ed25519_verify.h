// monday/security/ed25519_verify.h
//
// Ed25519 signature verification interface.
//
// Architecture §2.3: The model manifest trust chain is:
//   OS code signing → monday-core binary → embedded public key
//     → manifest Ed25519 signature → model file SHA-256 hashes
//
// The 32-byte Ed25519 public key is embedded as a compile-time constant
// kModelVerifyKey[32].  This interface provides the verification function
// used by model_verify.cpp.
//
// When MONDAY_HAVE_LIBSODIUM is defined, the implementation uses libsodium's
// crypto_sign_ed25519_verify_detached().  When it is not defined, the
// implementation is a safe stub that always returns MONDAY_ERR_NOT_SUPPORTED,
// causing model_verify to refuse to load any model — the secure-by-default
// behaviour for a build without crypto support.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// The embedded Ed25519 public key (32 bytes).
// Defined as a compile-time constant in ed25519_verify.c.
// v1.0 ships one hardcoded key (architecture §2.3, DEF-12).
extern const uint8_t kModelVerifyKey[32];

// Verify an Ed25519 signature.
//
// message      = message bytes to verify.
// message_len  = length of message in bytes.
// signature    = 64-byte Ed25519 signature (base64url decoded by caller).
// public_key   = 32-byte public key (use kModelVerifyKey for model manifests).
//
// Returns MONDAY_OK if the signature is valid.
// Returns MONDAY_ERR_MANIFEST_INVALID if the signature does not verify.
// Returns MONDAY_ERR_NOT_SUPPORTED if the implementation is a stub.
monday_error_t monday_ed25519_verify(const uint8_t *message,
                                      size_t         message_len,
                                      const uint8_t  signature[64],
                                      const uint8_t  public_key[32]);

#ifdef __cplusplus
}
#endif
