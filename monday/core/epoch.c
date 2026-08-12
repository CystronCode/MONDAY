#include "monday/core/epoch.h"
#include "monday/core/platform.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef MONDAY_LINUX
#include <sched.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

void epoch_init(monday_epoch_t *ep) {
    atomic_init(&ep->global_epoch, 0);
    for (unsigned int i = 0; i < EPOCH_MAX_READERS; ++i) {
        atomic_init(&ep->reader_epoch[i], UINT64_MAX);
    }
}

int epoch_read_enter(monday_epoch_t *ep) {
    uint64_t current_epoch = atomic_load_explicit(&ep->global_epoch, memory_order_acquire);

    for (unsigned int i = 0; i < EPOCH_MAX_READERS; ++i) {
        uint64_t expected = UINT64_MAX;
        if (atomic_compare_exchange_strong_explicit(&ep->reader_epoch[i], &expected, current_epoch,
                                                    memory_order_acq_rel, memory_order_relaxed)) {
            return (int)i;
        }
    }
    return -1;
}

void epoch_read_exit(monday_epoch_t *ep, int id) {
    if (id >= 0 && (unsigned int)id < EPOCH_MAX_READERS) {
        atomic_store_explicit(&ep->reader_epoch[id], UINT64_MAX, memory_order_release);
    }
}

static void epoch_yield(void) {
#ifdef MONDAY_LINUX
    sched_yield();
#elif defined(_WIN32)
    YieldProcessor();
#else
    /* no-op fallback */
#endif
}

bool epoch_reset_begin(monday_epoch_t *ep) {
    uint64_t new_epoch = atomic_fetch_add_explicit(&ep->global_epoch, 1, memory_order_acq_rel) + 1;

    const uint64_t timeout_us = EPOCH_RESET_TIMEOUT_US;

#if defined(_WIN32)
    LARGE_INTEGER freq, start, current;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
#else
    struct timespec start_ts, current_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
#endif

    while (true) {
        bool all_cleared = true;
        for (unsigned int i = 0; i < EPOCH_MAX_READERS; ++i) {
            uint64_t reader_val = atomic_load_explicit(&ep->reader_epoch[i], memory_order_acquire);
            if (reader_val != UINT64_MAX && reader_val < new_epoch) {
                all_cleared = false;
                break;
            }
        }
        if (all_cleared) {
            return true;
        }

#if defined(_WIN32)
        QueryPerformanceCounter(&current);
        uint64_t elapsed_us = (uint64_t)((current.QuadPart - start.QuadPart) * 1000000
                                          / freq.QuadPart);
        if (elapsed_us >= timeout_us) return false;
        Sleep(1);
#else
        clock_gettime(CLOCK_MONOTONIC, &current_ts);
        uint64_t elapsed_us = (uint64_t)((current_ts.tv_sec  - start_ts.tv_sec)  * 1000000ULL
                                        + (current_ts.tv_nsec - start_ts.tv_nsec) / 1000ULL);
        if (elapsed_us >= timeout_us) return false;
        struct timespec req = {0, 1000000}; /* 1 ms backoff */
        nanosleep(&req, NULL);
#endif
        epoch_yield();
    }
}

void epoch_reset_end(monday_epoch_t *ep) {
    (void)ep;
}
