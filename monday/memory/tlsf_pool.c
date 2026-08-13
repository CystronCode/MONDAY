#include "monday/memory/tlsf_pool.h"
#include "monday/core/platform.h"
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

#define FL_INDEX_MAX 27
#define SL_INDEX_COUNT 16
#define ALIGN_SIZE 16
#define MIN_BLOCK_SIZE 32

typedef struct block_header {
    struct block_header *prev_phys;
    size_t               size;
    struct block_header *next_free;
    struct block_header *prev_free;
} block_header_t;

struct monday_tlsf_pool {
    uint32_t       fl_bitmap;
    uint16_t       sl_bitmap[28];
    block_header_t *free_lists[28][16];
    size_t         free_blocks;
    size_t         total_blocks;
    void           *pool_base;
    size_t         pool_size;
};

// Simplified mapping functions for O(1) TLSF
static inline void mapping_insert(size_t size, int *fl, int *sl) {
    if (size < MIN_BLOCK_SIZE) {
        *fl = 0;
        *sl = (int)(size / 16);
    } else {
        int fl_idx = 31 - __builtin_clz((uint32_t)size);
        *fl = fl_idx - 4; // Shift for minimum size class
        if (*fl > FL_INDEX_MAX) *fl = FL_INDEX_MAX;
        *sl = (int)((size >> (fl_idx - 4)) & 0xF);
    }
}

static inline void mapping_search(size_t size, int *fl, int *sl) {
    size += (1 << (31 - __builtin_clz((uint32_t)size) - 4)) - 1;
    mapping_insert(size, fl, sl);
}

monday_tlsf_pool_t* monday_tlsf_init(void *memory, size_t size) {
    if (size < sizeof(struct monday_tlsf_pool) + MIN_BLOCK_SIZE) return NULL;
    
    monday_tlsf_pool_t *pool = (monday_tlsf_pool_t*)memory;
    memset(pool, 0, sizeof(struct monday_tlsf_pool));
    
    pool->pool_base = memory;
    pool->pool_size = size;
    pool->free_blocks = 1;
    pool->total_blocks = 1;
    
    block_header_t *initial_block = (block_header_t*)((uint8_t*)memory + sizeof(struct monday_tlsf_pool));
    initial_block->size = size - sizeof(struct monday_tlsf_pool) - sizeof(block_header_t);
    initial_block->size |= 2; // this_free = 1
    
    int fl, sl;
    mapping_insert(initial_block->size & ~3, &fl, &sl);
    pool->free_lists[fl][sl] = initial_block;
    pool->sl_bitmap[fl] |= (1 << sl);
    pool->fl_bitmap |= (1 << fl);
    
    return pool;
}

void* monday_tlsf_alloc(monday_tlsf_pool_t *pool, size_t size) {
    if (!pool || size == 0) return NULL;
    
    // Ensure min alignment
    size = (size + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1);
    if (size < MIN_BLOCK_SIZE) size = MIN_BLOCK_SIZE;
    
    int fl, sl;
    mapping_search(size, &fl, &sl);
    
    // Search bitmasks
    int fl_search = fl;
    int sl_search = sl;
    
    uint16_t sl_map = pool->sl_bitmap[fl_search] & (~0U << sl_search);
    if (!sl_map) {
        uint32_t fl_map = pool->fl_bitmap & (~0U << (fl_search + 1));
        if (!fl_map) return NULL;
        
        fl_search = __builtin_ctz(fl_map);
        sl_map = pool->sl_bitmap[fl_search];
    }
    
    sl_search = __builtin_ctz(sl_map);
    
    // Remove from free list
    block_header_t *block = pool->free_lists[fl_search][sl_search];
    pool->free_lists[fl_search][sl_search] = block->next_free;
    if (block->next_free) block->next_free->prev_free = NULL;
    else {
        pool->sl_bitmap[fl_search] &= ~(1 << sl_search);
        if (!pool->sl_bitmap[fl_search]) pool->fl_bitmap &= ~(1 << fl_search);
    }
    
    // Split block if possible
    size_t block_size = block->size & ~3;
    if (block_size >= size + MIN_BLOCK_SIZE + sizeof(block_header_t)) {
        block_header_t *rem = (block_header_t*)((uint8_t*)block + sizeof(block_header_t) + size);
        rem->size = (block_size - size - sizeof(block_header_t)) | 2; // this_free = 1
        rem->prev_phys = block;
        
        int r_fl, r_sl;
        mapping_insert(rem->size & ~3, &r_fl, &r_sl);
        
        rem->next_free = pool->free_lists[r_fl][r_sl];
        rem->prev_free = NULL;
        if (rem->next_free) rem->next_free->prev_free = rem;
        pool->free_lists[r_fl][r_sl] = rem;
        
        pool->sl_bitmap[r_fl] |= (1 << r_sl);
        pool->fl_bitmap |= (1 << r_fl);
        
        block->size = size; // no longer free
        pool->total_blocks++;
    } else {
        block->size &= ~2; // this_free = 0
        pool->free_blocks--;
    }
    
    return (uint8_t*)block + sizeof(block_header_t);
}

void monday_tlsf_free(monday_tlsf_pool_t *pool, void *ptr) {
    if (!pool || !ptr) return;
    
    block_header_t *block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    block->size |= 2; // this_free = 1
    
    // Coalescing logic (simplified)
    if (block->prev_phys && (block->prev_phys->size & 2)) {
        // Coalesce with prev
    }
    
    int fl, sl;
    mapping_insert(block->size & ~3, &fl, &sl);
    block->next_free = pool->free_lists[fl][sl];
    block->prev_free = NULL;
    if (block->next_free) block->next_free->prev_free = block;
    pool->free_lists[fl][sl] = block;
    
    pool->sl_bitmap[fl] |= (1 << sl);
    pool->fl_bitmap |= (1 << fl);
    pool->free_blocks++;
}

size_t monday_tlsf_free_block_count(const monday_tlsf_pool_t *pool) {
    return pool ? pool->free_blocks : 0;
}

size_t monday_tlsf_total_block_count(const monday_tlsf_pool_t *pool) {
    return pool ? pool->total_blocks : 0;
}

double monday_tlsf_free_ratio(const monday_tlsf_pool_t *pool) {
    if (!pool || pool->total_blocks == 0) return 1.0;
    return (double)pool->free_blocks / (double)pool->total_blocks;
}

bool monday_tlsf_rebuild(monday_tlsf_pool_t *pool) {
    // TODO: Rebuild logic for Phase 3+
    (void)pool;
    return true;
}
