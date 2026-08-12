// monday/memory/zone.h
//
// Named zone management layer above allocator.c.
// Provides a higher-level API for subsystems that need per-turn scratch
// arenas without directly coupling to the zone-type enum.

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque zone handle.
typedef struct monday_named_zone monday_named_zone_t;

// Create a named sub-arena within PU_LEVEL.
// name       = human-readable label for diagnostics (copied internally).
// reserved   = hint bytes that will be used this turn; may be 0.
// Returns a non-NULL handle, or NULL on allocation failure.
monday_named_zone_t *monday_zone_create(const char *name, size_t reserved);

// Allocate from a named zone.  Backed by PU_LEVEL bump allocator.
void *monday_zone_named_alloc(monday_named_zone_t *zone, size_t size);

// Release all memory in a named zone (resets its internal offset).
// Does NOT release the zone handle itself — call monday_zone_destroy for that.
void monday_zone_named_reset(monday_named_zone_t *zone);

// Destroy a named zone handle.  The zone's memory is automatically
// reclaimed when the PU_LEVEL arena is reset by monday_level_reset().
void monday_zone_destroy(monday_named_zone_t *zone);

// Query current bytes used in a named zone.
size_t monday_zone_bytes_used(const monday_named_zone_t *zone);

#ifdef __cplusplus
}
#endif
