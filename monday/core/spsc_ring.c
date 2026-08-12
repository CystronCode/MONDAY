// monday/core/spsc_ring.c
//
// Lock-free Single-Producer Single-Consumer ring buffer implementation.

#include "monday/core/spsc_ring.h"
#include "monday/core/allocator.h"
#include <string.h>

monday_error_t monday_spsc_ring_init(monday_spsc_ring_t *ring,
                                     size_t capacity,
                                     size_t element_size) {
    if (!ring || capacity == 0 || element_size == 0) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid spsc ring init arguments");
    }

    /* Verify capacity is power of two */
    if ((capacity & (capacity - 1)) != 0) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "spsc ring capacity must be power of two");
    }

    ring->buffer = (uint8_t *)monday_zone_alloc(MONDAY_ZONE_STATIC, capacity * element_size);
    if (!ring->buffer) {
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to allocate spsc ring buffer");
    }

    ring->element_size = element_size;
    ring->capacity     = capacity;
    ring->mask         = capacity - 1;
    atomic_init(&ring->head, 0);
    atomic_init(&ring->tail, 0);

    return MONDAY_ERROR(MONDAY_OK, "spsc ring initialized");
}

void monday_spsc_ring_free(monday_spsc_ring_t *ring) {
    if (!ring) return;
    ring->buffer = NULL;
    atomic_store(&ring->head, 0);
    atomic_store(&ring->tail, 0);
}

monday_error_t monday_spsc_ring_push(monday_spsc_ring_t *ring, const void *element) {
    if (!ring || !element) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null argument to spsc ring push");
    }

    size_t tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&ring->head, memory_order_acquire);

    if (tail - head >= ring->capacity) {
        return MONDAY_ERROR(MONDAY_ERR_ARENA_FULL, "spsc ring full");
    }

    size_t offset = (tail & ring->mask) * ring->element_size;
    memcpy(ring->buffer + offset, element, ring->element_size);

    atomic_store_explicit(&ring->tail, tail + 1, memory_order_release);
    return MONDAY_ERROR(MONDAY_OK, "push success");
}

monday_error_t monday_spsc_ring_pop(monday_spsc_ring_t *ring, void *out_element) {
    if (!ring || !out_element) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null argument to spsc ring pop");
    }

    size_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);

    if (head == tail) {
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "spsc ring empty");
    }

    size_t offset = (head & ring->mask) * ring->element_size;
    memcpy(out_element, ring->buffer + offset, ring->element_size);

    atomic_store_explicit(&ring->head, head + 1, memory_order_release);
    return MONDAY_ERROR(MONDAY_OK, "pop success");
}

size_t monday_spsc_ring_count(const monday_spsc_ring_t *ring) {
    if (!ring) return 0;
    size_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    return (tail >= head) ? (tail - head) : 0;
}

size_t monday_spsc_ring_capacity(const monday_spsc_ring_t *ring) {
    return ring ? ring->capacity : 0;
}
