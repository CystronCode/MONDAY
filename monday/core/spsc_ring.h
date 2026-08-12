// monday/core/spsc_ring.h
//
// Single-Producer Single-Consumer (SPSC) lock-free ring buffer.
// Zero heap allocation on push/pop. Thread-safe for 1 producer thread + 1 consumer thread.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "monday/core/errors.h"

#ifdef __cplusplus
#include <atomic>
extern "C" {
typedef std::atomic<size_t> monday_atomic_size_t;
#else
#include <stdatomic.h>
typedef _Atomic size_t monday_atomic_size_t;
#endif

typedef struct monday_spsc_ring {
    uint8_t *buffer;
    size_t   element_size;
    size_t   capacity;       /* Must be power of two */
    size_t   mask;           /* capacity - 1 */
    monday_atomic_size_t head;     /* Written by consumer, read by producer */
    monday_atomic_size_t tail;     /* Written by producer, read by consumer */
} monday_spsc_ring_t;

/* Initialize SPSC ring buffer. Capacity MUST be a power of two. */
monday_error_t monday_spsc_ring_init(monday_spsc_ring_t *ring,
                                     size_t capacity,
                                     size_t element_size);

/* Destroy SPSC ring buffer. */
void monday_spsc_ring_free(monday_spsc_ring_t *ring);

/* Push an element into the ring (Producer thread only).
 * Non-blocking. Returns MONDAY_ERR_ARENA_FULL if full. */
monday_error_t monday_spsc_ring_push(monday_spsc_ring_t *ring, const void *element);

/* Pop an element from the ring (Consumer thread only).
 * Non-blocking. Returns MONDAY_ERR_NOT_FOUND if empty. */
monday_error_t monday_spsc_ring_pop(monday_spsc_ring_t *ring, void *out_element);

/* Current number of elements in the ring. */
size_t monday_spsc_ring_count(const monday_spsc_ring_t *ring);

/* Capacity of the ring. */
size_t monday_spsc_ring_capacity(const monday_spsc_ring_t *ring);

#ifdef __cplusplus
}
#endif
