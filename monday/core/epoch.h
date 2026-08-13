// monday/core/epoch.h
//
// Epoch-based reclamation for arena reset synchronisation.
//
// Protocol (per architecture §2.4):
//   Readers call epoch_read_enter() before touching arena memory and
//   epoch_read_exit() immediately after. The resetter calls epoch_reset_begin()
//   which blocks (briefly) until every reader has exited its critical section,
//   then calls epoch_reset_end() when the arena is reconstructed.
//
// Max concurrent readers is fixed at EPOCH_MAX_READERS (compile-time).
// Thread-safety: all functions are async-signal safe except epoch_reset_begin
//   which may spin.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "monday/core/platform.h"
#ifndef _MSC_VER
#include <stdatomic.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define EPOCH_MAX_READERS 16u

// Opaque epoch state.  Zero-initialise before first use or call epoch_init().
typedef struct monday_epoch {
    atomic_uint_fast64_t global_epoch;                    // monotonically increasing
    atomic_uint_fast64_t reader_epoch[EPOCH_MAX_READERS]; // each reader's last seen epoch
    // UINT64_MAX means "no reader registered in this slot"
} monday_epoch_t;

// Initialise an epoch object.  Must be called before any other function.
void epoch_init(monday_epoch_t *ep);

// Acquire a reader slot.  Returns a reader_id in [0, EPOCH_MAX_READERS) on
// success, -1 if all slots are taken.  Callers must release via epoch_read_exit.
int epoch_read_enter(monday_epoch_t *ep);

// Release the reader slot acquired by epoch_read_enter.
// reader_id must be the value returned by epoch_read_enter.
void epoch_read_exit(monday_epoch_t *ep, int reader_id);

// Called by the arena resetter before resetting.
// Increments the global epoch and spins until all active readers have exited
// their critical sections (their stored epoch catches up to the new epoch).
// Spin is bounded by a brief backoff; does not block indefinitely on well-
// behaved readers.
// Returns true on success.
// Returns false if spinning exceeded EPOCH_RESET_TIMEOUT_US microseconds,
// which maps to MONDAY_ERR_ARENA_RESET_BUSY.
bool epoch_reset_begin(monday_epoch_t *ep);

// Called by the arena resetter after the reset is complete.
// Currently a no-op but kept for symmetry and future extension.
void epoch_reset_end(monday_epoch_t *ep);

// Maximum spin-wait before epoch_reset_begin returns false (10 seconds).
#define EPOCH_RESET_TIMEOUT_US 10000000u

#ifdef __cplusplus
}
#endif
