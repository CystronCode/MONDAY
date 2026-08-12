#include "monday/memory/zone.h"
#include "monday/core/allocator.h"
#include <string.h>
#include <stddef.h>

struct monday_named_zone {
    char   name[64];     // diagnostic label
    size_t base_offset;  // offset into PU_LEVEL at creation time
    size_t used;         // bytes allocated from this sub-zone
};

monday_named_zone_t* monday_zone_create(const char *name, size_t reserved) {
    (void)reserved; // Not explicitly handled in sub-zone creation per phase 1 requirements
    
    monday_named_zone_t *zone = (monday_named_zone_t*)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_named_zone_t));
    if (!zone) return NULL;
    
    if (name) {
        strncpy(zone->name, name, sizeof(zone->name) - 1);
        zone->name[sizeof(zone->name) - 1] = '\0';
    } else {
        zone->name[0] = '\0';
    }
    
    monday_allocator_stats_t stats;
    monday_allocator_stats(&stats);
    
    zone->base_offset = stats.level_used;
    zone->used = 0;
    
    return zone;
}

void* monday_zone_named_alloc(monday_named_zone_t *zone, size_t size) {
    if (!zone) return NULL;
    
    void *ptr = monday_zone_alloc(MONDAY_ZONE_LEVEL, size);
    if (ptr) {
        zone->used += size;
    }
    
    return ptr;
}

void monday_zone_named_reset(monday_named_zone_t *zone) {
    if (zone) {
        zone->used = 0;
    }
}

void monday_zone_destroy(monday_named_zone_t *zone) {
    if (zone) {
        zone->name[0] = '\0';
    }
}

size_t monday_zone_bytes_used(const monday_named_zone_t *zone) {
    if (!zone) return 0;
    return zone->used;
}
