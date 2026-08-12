// monday/security/model_verify.h
//
// Model manifest verification — SHA-256 hash check before mmap.
//
// Architecture §2.3: The manifest file (manifest.json, co-located with model.bin)
// carries Ed25519 signature over the canonical JSON of the "files" array.
// Each file entry carries a SHA-256 hex hash.  This module:
//   1. Parses manifest.json.
//   2. Verifies the Ed25519 signature against kModelVerifyKey.
//   3. Computes SHA-256 of each model file on disk.
//   4. Compares against the manifest-declared hashes.
// Only after all checks pass is mmap of the model file permitted.
//
// The OS keystore entry (SHA-256 of the manifest file) is verified by the
// keystore module — this module handles only the manifest itself.

#pragma once

#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// Result of manifest verification.
typedef struct {
    char  model_path[4096];   // resolved absolute path to model file
    char  sha256_hex[65];     // expected SHA-256 (null-terminated hex, 64 chars + NUL)
    long  size_bytes;         // expected size in bytes
} monday_model_manifest_t;

// Verify a model manifest file.
//
// manifest_path  = path to manifest.json.
// out            = populated on MONDAY_OK; zeroed otherwise.
//
// Returns MONDAY_OK if:
//   - manifest.json parses correctly,
//   - Ed25519 signature is valid,
//   - model file exists, has correct size, and SHA-256 matches.
//
// Returns MONDAY_ERR_MANIFEST_INVALID   if manifest is missing or malformed.
// Returns MONDAY_ERR_MODEL_HASH_MISMATCH if SHA-256 of model file does not match.
// Returns MONDAY_ERR_NOT_FOUND          if model file is absent.
// Returns MONDAY_ERR_NOT_SUPPORTED      if crypto support is unavailable (stub build).
monday_error_t monday_model_verify(const char               *manifest_path,
                                    monday_model_manifest_t  *out);

#ifdef __cplusplus
}
#endif
