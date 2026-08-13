// monday/core/platform.h
#pragma once

#if defined(__linux__)
    #define MONDAY_LINUX 1
#elif defined(_WIN32) || defined(_WIN64)
    #define MONDAY_WINDOWS 1
#else
    #error "Unsupported platform — Monday supports Linux and Windows only."
#endif

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <stddef.h>

/* ── Bit scan intrinsics ─────────────────────────────────────────────────── */

static inline int monday_builtin_clz(unsigned int x) {
    unsigned long index;
    if (_BitScanReverse(&index, x)) return 31 - (int)index;
    return 32;
}

static inline int monday_builtin_ctz(unsigned int x) {
    unsigned long index;
    if (_BitScanForward(&index, x)) return (int)index;
    return 32;
}

#define __builtin_clz monday_builtin_clz
#define __builtin_ctz monday_builtin_ctz

/* ── _Alignof and max_align_t compat ────────────────────────────────────── */
#ifndef __cplusplus
#ifndef _Alignof
#define _Alignof __alignof
#endif
/* max_align_t is not exposed in MSVC C mode — define it as the largest
   fundamental alignment (16 bytes on x64). */
typedef double max_align_t;
#endif

/* ── C11 Atomics polyfill for MSVC C compiler ───────────────────────────── */
#ifndef __cplusplus

/* memory_order values (ignored at runtime — MSVC uses full barriers) */
#define memory_order_relaxed 0
#define memory_order_consume 1
#define memory_order_acquire 2
#define memory_order_release 3
#define memory_order_acq_rel 4
#define memory_order_seq_cst 5

/* atomic_size_t — used in zone_t.offset */
typedef volatile size_t atomic_size_t;

/* atomic_uint_fast64_t — used in monday_epoch_t */
typedef volatile uint64_t atomic_uint_fast64_t;

/* ── atomic_size_t operations ─────────────────────────────────────────────── */

static inline void atomic_init_sz(volatile size_t *obj, size_t val) {
    *obj = val;
}

static inline size_t atomic_load_explicit_sz(volatile size_t *obj, int order) {
    (void)order;
    return *obj;
}

static inline void atomic_store_explicit_sz(volatile size_t *obj, size_t val, int order) {
    (void)order;
#if defined(_WIN64)
    InterlockedExchange64((volatile LONG64*)obj, (LONG64)val);
#else
    InterlockedExchange((volatile LONG*)obj, (LONG)val);
#endif
}

static inline size_t atomic_fetch_add_explicit_sz(volatile size_t *obj, size_t arg, int order) {
    (void)order;
#if defined(_WIN64)
    return (size_t)InterlockedExchangeAdd64((volatile LONG64*)obj, (LONG64)arg);
#else
    return (size_t)InterlockedExchangeAdd((volatile LONG*)obj, (LONG)arg);
#endif
}

static inline size_t atomic_fetch_sub_explicit_sz(volatile size_t *obj, size_t arg, int order) {
    (void)order;
#if defined(_WIN64)
    return (size_t)InterlockedExchangeAdd64((volatile LONG64*)obj, -(LONG64)arg);
#else
    return (size_t)InterlockedExchangeAdd((volatile LONG*)obj, -(LONG)arg);
#endif
}

/* ── atomic_uint_fast64_t operations ─────────────────────────────────────── */

static inline void atomic_init_u64(volatile uint64_t *obj, uint64_t val) {
    *obj = val;
}

static inline uint64_t atomic_load_explicit_u64(volatile uint64_t *obj, int order) {
    (void)order;
    return *obj;
}

static inline void atomic_store_explicit_u64(volatile uint64_t *obj, uint64_t val, int order) {
    (void)order;
    InterlockedExchange64((volatile LONG64*)obj, (LONG64)val);
}

static inline uint64_t atomic_fetch_add_explicit_u64(volatile uint64_t *obj, uint64_t arg, int order) {
    (void)order;
    return (uint64_t)InterlockedExchangeAdd64((volatile LONG64*)obj, (LONG64)arg);
}

static inline _Bool atomic_compare_exchange_strong_explicit_u64(
    volatile uint64_t *obj, uint64_t *expected, uint64_t desired, int succ, int fail) {
    (void)succ; (void)fail;
    uint64_t old = (uint64_t)InterlockedCompareExchange64(
        (volatile LONG64*)obj, (LONG64)desired, (LONG64)*expected);
    if (old == *expected) return 1;
    *expected = old;
    return 0;
}

/* Route all atomic_* calls — callers pass either volatile size_t* or volatile uint64_t* */
#define atomic_init(obj, val)                           \
    (sizeof(*(obj)) == sizeof(uint64_t)                 \
        ? atomic_init_u64((volatile uint64_t*)(uintptr_t)(obj), (uint64_t)(val)) \
        : (void)(*(volatile size_t*)(uintptr_t)(obj) = (val)))

#define atomic_load_explicit(obj, order)                \
    (sizeof(*(obj)) == sizeof(uint64_t)                 \
        ? atomic_load_explicit_u64((volatile uint64_t*)(uintptr_t)(obj), (order)) \
        : (size_t)atomic_load_explicit_sz((volatile size_t*)(uintptr_t)(obj), (order)))

#define atomic_store_explicit(obj, val, order)          \
    (sizeof(*(obj)) == sizeof(uint64_t)                 \
        ? atomic_store_explicit_u64((volatile uint64_t*)(uintptr_t)(obj), (uint64_t)(val), (order)) \
        : atomic_store_explicit_sz((volatile size_t*)(uintptr_t)(obj), (size_t)(val), (order)))

#define atomic_store(obj, val)                          \
    atomic_store_explicit((obj), (val), memory_order_seq_cst)

#define atomic_fetch_add_explicit(obj, arg, order)      \
    (sizeof(*(obj)) == sizeof(uint64_t)                 \
        ? atomic_fetch_add_explicit_u64((volatile uint64_t*)(uintptr_t)(obj), (uint64_t)(arg), (order)) \
        : atomic_fetch_add_explicit_sz((volatile size_t*)(uintptr_t)(obj), (size_t)(arg), (order)))

#define atomic_fetch_sub_explicit(obj, arg, order)      \
    atomic_fetch_sub_explicit_sz((volatile size_t*)(uintptr_t)(obj), (size_t)(arg), (order))

#define atomic_compare_exchange_strong_explicit(obj, expected, desired, succ, fail) \
    atomic_compare_exchange_strong_explicit_u64(        \
        (volatile uint64_t*)(uintptr_t)(obj), (uint64_t*)(expected), (uint64_t)(desired), (succ), (fail))

#endif /* !__cplusplus */
#endif /* _MSC_VER */
