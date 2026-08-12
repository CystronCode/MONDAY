// monday-core/shutdown.h
//
// SIGTERM / SIGABRT signal handler: key zeroing before process exit.
//
// Architecture §3.7: On crash, a key zeroing handler runs as signal handler on
// SIGTERM/SIGABRT before process exit.  The handler must be async-signal safe
// and must not call malloc, printf, or any function that could deadlock.

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Install signal handlers for SIGTERM and SIGABRT.
// Call once at daemon startup before any key material is derived.
void monday_shutdown_init(void);

// Trigger a clean shutdown from any thread.
// - Zeroes any mlock()'d key material regions.
// - Writes a final NDJSON log line: {"level":"SYSTEM","msg":"monday-core shutdown"}.
// - Calls exit(0).
// This function does NOT return.
void monday_shutdown_clean(void);

// Register a key buffer for zeroing on shutdown.
// key_ptr  = pointer to key material (must remain valid until shutdown).
// key_len  = length in bytes.
// Supports up to MONDAY_SHUTDOWN_MAX_KEYS registrations.
void monday_shutdown_register_key(void *key_ptr, size_t key_len);

// Maximum key buffers that can be registered.
#define MONDAY_SHUTDOWN_MAX_KEYS 8

#ifdef __cplusplus
}
#endif
