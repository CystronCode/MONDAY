#include "monday/core/allocator.h"
#include "monday/core/platform.h"
#include "monday/core/epoch.h"
#include "monday/memory/tlsf_pool.h"

#include <stddef.h>
#include <stdint.h>
#ifndef _MSC_VER
#include <stdatomic.h>
#endif
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

/* ── Internal zone structure ─────────────────────────────────────────────── */

typedef struct {
    uint8_t        *base;
    size_t          capacity;
    atomic_size_t   offset;      /* bump pointer; atomic for lock-free alloc */
    size_t          high_water;  /* peak offset ever reached                 */
} zone_t;

static zone_t g_static = {NULL, 0, 0, 0};
static zone_t g_level  = {NULL, 0, 0, 0};
static zone_t g_cache  = {NULL, 0, 0, 0};

static void  *g_base_ptr   = NULL;
static size_t g_total_size = 0;

monday_epoch_t      g_level_epoch;
monday_tlsf_pool_t *g_tlsf = NULL;

/* ── Allocator lifecycle ─────────────────────────────────────────────────── */

monday_error_t monday_allocator_init(size_t static_size,
                                      size_t level_size,
                                      size_t cache_size,
                                      size_t tlsf_size) {
    g_total_size = static_size + level_size + cache_size + tlsf_size;

#if defined(_WIN32)
    g_base_ptr = VirtualAlloc(NULL, g_total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    g_base_ptr = mmap(NULL, g_total_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_base_ptr == MAP_FAILED) g_base_ptr = NULL;
#endif

    if (!g_base_ptr) {
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "allocator mmap failed");
    }

    uint8_t *ptr = (uint8_t *)g_base_ptr;

    g_static.base = ptr;  g_static.capacity = static_size;
    atomic_init(&g_static.offset, 0);  g_static.high_water = 0;
    ptr += static_size;

    g_level.base = ptr;   g_level.capacity = level_size;
    atomic_init(&g_level.offset, 0);   g_level.high_water = 0;
    ptr += level_size;

    g_cache.base = ptr;   g_cache.capacity = cache_size;
    atomic_init(&g_cache.offset, 0);   g_cache.high_water = 0;
    ptr += cache_size;

    epoch_init(&g_level_epoch);

    if (tlsf_size > 0) {
        g_tlsf = monday_tlsf_init(ptr, tlsf_size);
    }

    return MONDAY_ERROR(MONDAY_OK, "");
}

void monday_allocator_shutdown(void) {
    if (g_base_ptr) {
#if defined(_WIN32)
        VirtualFree(g_base_ptr, 0, MEM_RELEASE);
#else
        munmap(g_base_ptr, g_total_size);
#endif
        g_base_ptr = NULL;
        g_tlsf     = NULL;
    }
}

/* ── Zone allocations (bump-pointer, O(1)) ──────────────────────────────── */

void *monday_zone_alloc(monday_zone_type_t zone_type, size_t size) {
    zone_t *zone = NULL;
    switch (zone_type) {
        case MONDAY_ZONE_STATIC: zone = &g_static; break;
        case MONDAY_ZONE_LEVEL:  zone = &g_level;  break;
        case MONDAY_ZONE_CACHE:  zone = &g_cache;  break;
        default: return NULL;
    }

    /* Align to _Alignof(max_align_t) — C11 standard guarantee */
    size_t align = _Alignof(max_align_t);
    size = (size + align - 1u) & ~(align - 1u);

    size_t cur = atomic_fetch_add_explicit(&zone->offset, size, memory_order_relaxed);
    if (cur + size > zone->capacity) {
        /* Roll back: zone exhausted */
        atomic_fetch_sub_explicit(&zone->offset, size, memory_order_relaxed);
        return NULL;
    }

    if (cur + size > zone->high_water) {
        zone->high_water = cur + size;
    }

    return zone->base + cur;
}

/* ── PU_LEVEL reset ─────────────────────────────────────────────────────── */

monday_error_t monday_level_reset(void) {
    if (!epoch_reset_begin(&g_level_epoch)) {
        return MONDAY_ERROR(MONDAY_ERR_ARENA_RESET_BUSY, "epoch timeout in level reset");
    }

    atomic_store_explicit(&g_level.offset, 0, memory_order_release);

#if defined(__SANITIZE_ADDRESS__)
    ASAN_POISON_MEMORY_REGION(g_level.base, g_level.capacity);
#endif

    epoch_reset_end(&g_level_epoch);
    return MONDAY_ERROR(MONDAY_OK, "");
}

/* ── PU_CACHE eviction ──────────────────────────────────────────────────── */

void monday_cache_evict(void) {
    atomic_store_explicit(&g_cache.offset, 0, memory_order_relaxed);
}

/* ── TLSF general-purpose allocations ──────────────────────────────────── */

void *monday_alloc(size_t size) {
    if (!g_tlsf) return NULL;
    return monday_tlsf_alloc(g_tlsf, size);
}

void monday_free(void *ptr) {
    if (!g_tlsf || !ptr) return;
    monday_tlsf_free(g_tlsf, ptr);
}

/* ── Stats ──────────────────────────────────────────────────────────────── */

void monday_allocator_stats(monday_allocator_stats_t *out) {
    if (!out) return;

    out->static_used      = atomic_load_explicit(&g_static.offset, memory_order_relaxed);
    out->static_capacity  = g_static.capacity;

    out->level_used       = atomic_load_explicit(&g_level.offset, memory_order_relaxed);
    out->level_capacity   = g_level.capacity;
    out->level_high_water = g_level.high_water;

    out->cache_used       = atomic_load_explicit(&g_cache.offset, memory_order_relaxed);
    out->cache_capacity   = g_cache.capacity;

    if (g_tlsf) {
        out->tlsf_free_blocks  = monday_tlsf_free_block_count(g_tlsf);
        out->tlsf_total_blocks = monday_tlsf_total_block_count(g_tlsf);
        out->tlsf_free_ratio   = monday_tlsf_free_ratio(g_tlsf);
    } else {
        out->tlsf_free_blocks  = 0;
        out->tlsf_total_blocks = 0;
        out->tlsf_free_ratio   = 1.0;
    }
}

/* ── Epoch handle ───────────────────────────────────────────────────────── */

struct monday_epoch *monday_level_epoch(void) {
    return &g_level_epoch;
}
