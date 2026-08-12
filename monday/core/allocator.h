// monday/core/allocator.h
//
// DOOM Zone Allocator — PU_STATIC / PU_LEVEL / PU_CACHE arena allocator.
// TLSF is used for individually-freeable allocations (monday_alloc / monday_free).
//
// Zone semantics (architecture §2.4):
//   PU_STATIC  — permanent; freed only at process exit.
//   PU_LEVEL   — per-turn; reset atomically at monday_blackboard_turn_begin().
//                Epoch-based reclamation prevents use-after-free during reset.
//                After reset, the freed region is poisoned with
//                ASAN_POISON_MEMORY_REGION so that ASan catches stale reads.
//   PU_CACHE   — evictable under memory pressure; not freed individually.
//
// monday_alloc() / monday_free() route through the TLSF general-purpose pool.
// monday_zone_alloc() bumps the arena pointer for the named zone.
//
// Init order: monday_allocator_init() must be called before any allocation.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Zone types ───────────────────────────────────────────────────────────────

typedef enum {
    MONDAY_ZONE_STATIC = 0,  // PU_STATIC — never reset
    MONDAY_ZONE_LEVEL  = 1,  // PU_LEVEL  — reset per turn
    MONDAY_ZONE_CACHE  = 2,  // PU_CACHE  — evicted under pressure
} monday_zone_type_t;

// ── Allocator lifecycle ───────────────────────────────────────────────────────

// Initialise the global allocator.
// static_size  = bytes reserved for PU_STATIC zone.
// level_size   = bytes reserved for PU_LEVEL zone.
// cache_size   = bytes reserved for PU_CACHE zone.
// tlsf_size    = bytes for the TLSF general-purpose pool.
// All memory is mmap()'d as a contiguous anonymous mapping.
// Returns MONDAY_OK or MONDAY_ERR_OUT_OF_MEMORY.
monday_error_t monday_allocator_init(size_t static_size,
                                      size_t level_size,
                                      size_t cache_size,
                                      size_t tlsf_size);

// Tear down the allocator and unmap all memory.  Call only at process exit.
void monday_allocator_shutdown(void);

// ── Zone allocations (bump-pointer, O(1)) ────────────────────────────────────

// Bump-allocate from a named zone. Alignment is guaranteed to sizeof(max_align_t).
// Returns NULL only on zone exhaustion (MONDAY_ERR_ARENA_FULL is also set in
// the thread-local error register).
void *monday_zone_alloc(monday_zone_type_t zone, size_t size);

// Reset the PU_LEVEL zone.  Blocks via epoch-based reclamation until no reader
// holds a pointer into the level arena.  After reset the freed region is
// poisoned (ASan) so stale reads are detected immediately.
// Returns MONDAY_ERR_ARENA_RESET_BUSY if epoch timeout elapses.
monday_error_t monday_level_reset(void);

// Evict the PU_CACHE zone (pointer reset without epoch guard — callers of
// PU_CACHE are expected to tolerate eviction).
void monday_cache_evict(void);

// ── TLSF general-purpose allocations (individually freeable) ─────────────────

// Allocate from the TLSF pool.  O(1) worst case.  Thread-safe.
// Used for InferenceResult.text, IPC message buffers, etc.
void *monday_alloc(size_t size);

// Free a pointer previously returned by monday_alloc().
// Passing a zone-allocated pointer is undefined behaviour.
void monday_free(void *ptr);

// ── Stats ─────────────────────────────────────────────────────────────────────

typedef struct {
    size_t static_used;      // bytes consumed in PU_STATIC
    size_t static_capacity;
    size_t level_used;       // bytes consumed in PU_LEVEL (current turn)
    size_t level_capacity;
    size_t level_high_water; // peak bytes used in PU_LEVEL across all turns
    size_t cache_used;
    size_t cache_capacity;
    size_t tlsf_free_blocks;
    size_t tlsf_total_blocks;
    double tlsf_free_ratio;  // tlsf_free_blocks / tlsf_total_blocks
} monday_allocator_stats_t;

void monday_allocator_stats(monday_allocator_stats_t *out);

// Epoch handle exposed so zone.c / tests can enter/exit read sections.
struct monday_epoch; // forward decl
struct monday_epoch *monday_level_epoch(void);

#ifdef __cplusplus
}
#endif
