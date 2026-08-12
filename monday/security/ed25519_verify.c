#include "monday/security/ed25519_verify.h"
#include <string.h>

// Ed25519 public key embedded at compile time.
// In stub/dev builds: all-zero key (not a valid key; verifier will reject all).
// In release builds: the real 32-byte key is embedded by the hermetic build
// container as a compile-time constant per architecture §2.3 (DEF-12).
const uint8_t kModelVerifyKey[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#ifdef MONDAY_HAVE_LIBSODIUM
#include <sodium.h>

monday_error_t monday_ed25519_verify(const uint8_t *message,
                                      size_t         message_len,
                                      const uint8_t  signature[64],
                                      const uint8_t  public_key[32]) {
    if (!message || !signature || !public_key) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null argument to ed25519_verify");
    }
    int rc = crypto_sign_ed25519_verify_detached(signature, message,
                                                  (unsigned long long)message_len,
                                                  public_key);
    if (rc != 0) {
        return MONDAY_ERROR(MONDAY_ERR_MANIFEST_INVALID, "Ed25519 signature invalid");
    }
    return MONDAY_ERROR(MONDAY_OK, "");
}

#else  // !MONDAY_HAVE_LIBSODIUM

// Stub: no crypto support. Refuse all verification.
// This is the secure-by-default behaviour: a build without libsodium cannot
// load any model because it cannot verify the manifest signature.
// Tests that need to exercise the verification path must define MONDAY_HAVE_LIBSODIUM
// or use the test-only bypass described in model_verify.h.
monday_error_t monday_ed25519_verify(const uint8_t *message,
                                      size_t         message_len,
                                      const uint8_t  signature[64],
                                      const uint8_t  public_key[32]) {
    (void)message; (void)message_len; (void)signature; (void)public_key;
    return MONDAY_ERROR(MONDAY_ERR_NOT_SUPPORTED,
                        "Ed25519 verify not available: build without MONDAY_HAVE_LIBSODIUM");
}

#endif  // MONDAY_HAVE_LIBSODIUM
