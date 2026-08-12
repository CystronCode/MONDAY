// monday/security/keystore.h
//
// Secure Keystore interface for salt and key storage.
// Architecture §3.7: Handles DPAPI (Windows) or Linux keyring/file storage.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// Set a key/value pair in the secure keystore.
// name: key name (e.g. "monday.db.salt" or "monday.db.key")
// data: pointer to key data
// len: length of key data
monday_error_t monday_keystore_set(const char *name, const uint8_t *data, size_t len);

// Get a key/value pair from the secure keystore.
// name: key name
// out_data: buffer to write key data
// len: expected length of key data
monday_error_t monday_keystore_get(const char *name, uint8_t *out_data, size_t len);

// Delete a key/value pair from the secure keystore.
// name: key name
monday_error_t monday_keystore_delete(const char *name);

#ifdef __cplusplus
}
#endif
