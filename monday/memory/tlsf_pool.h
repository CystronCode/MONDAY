// monday/memory/tlsf_pool.h
//
// TLSF (Two-Level Segregated Fit) general-purpose allocator pool wrapper.
//
// Architecture §2.4: TLSF provides O(1) worst-case allocation and deallocation
// for use cases where DOOM zones cannot apply (SQLite internal buffers, IPC
// message buffers, plugin subprocess communication).
//
// Free-block ratio monitoring: when tlsf_free_block_count() / tlsf_total_blocks()
// falls below 0.15, monday_tlsf_maybe_rebuild() schedules a controlled pool
// drain during the next idle period.
//
// This header is the internal interface used by allocator.c.  External callers
// use monday_alloc() / monday_free() from allocator.h.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque TLSF pool handle.
typedef struct monday_tlsf_pool monday_tlsf_pool_t;

// Initialise a TLSF pool over an existing memory region.
// memory  = pointer to pre-allocated backing store (mmap'd).
// size    = size in bytes.  Must be >= MONDAY_TLSF_MIN_SIZE.
// Returns non-NULL on success, NULL on failure (size too small).
monday_tlsf_pool_t *monday_tlsf_init(void *memory, size_t size);

// Allocate size bytes from the pool.  O(1) worst-case.  Returns NULL if
// the pool has no suitable free block.
void *monday_tlsf_alloc(monday_tlsf_pool_t *pool, size_t size);

// Free a pointer previously returned by monday_tlsf_alloc().
// Passing a pointer not from this pool is undefined behaviour.
void monday_tlsf_free(monday_tlsf_pool_t *pool, void *ptr);

// Return number of free blocks currently tracked by the pool.
size_t monday_tlsf_free_block_count(const monday_tlsf_pool_t *pool);

// Return total number of blocks (free + used) tracked by the pool.
size_t monday_tlsf_total_block_count(const monday_tlsf_pool_t *pool);

// Compute the free-block ratio.  Returns value in [0.0, 1.0].
// The architecture triggers a pool rebuild when this falls below 0.15.
double monday_tlsf_free_ratio(const monday_tlsf_pool_t *pool);

// Perform a full pool rebuild.  All live allocations are walked,
// serialised to a temporary buffer, the pool is recreated from the
// same backing store, and live allocations are restored.
// This is NOT compaction — TLSF cannot move raw pointers.
// It is a full pool recreate to eliminate fragmented free-block structure.
// MUST be called only during idle time with no concurrent allocators.
// Returns true on success.
bool monday_tlsf_rebuild(monday_tlsf_pool_t *pool);

// Minimum pool size in bytes.
#define MONDAY_TLSF_MIN_SIZE (4096u)

// Fragmentation rebuild threshold (15% free-block ratio).
#define MONDAY_TLSF_REBUILD_THRESHOLD 0.15

#ifdef __cplusplus
}
#endif
