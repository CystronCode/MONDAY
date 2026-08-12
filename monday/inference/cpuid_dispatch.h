// monday/inference/cpuid_dispatch.h
//
// Runtime CPUID feature detection and kernel dispatch.
//
// Architecture §2.2: Release builds use -march=x86-64-v2 as the portable
// baseline. AVX2 and SSE4.2 kernel variants are selected at runtime via
// __builtin_cpu_supports(). Scalar fallback handles CPUs below SSE4.2.
//
// Dispatch is performed once at startup; the selected function pointer is
// stored globally.  Per-call overhead is a single indirect call — zero CPUID
// cost on the hot path.

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Feature flags detected at runtime.
typedef enum {
    MONDAY_CPU_SCALAR = 0,
    MONDAY_CPU_SSE4_2 = 1,
    MONDAY_CPU_AVX2   = 2,
} monday_cpu_features_t;

// Detect the highest available SIMD level on this CPU.
// Called once at startup.  Result is cached in g_cpu_features.
monday_cpu_features_t monday_cpuid_detect(void);

// Globally cached result.  Set by monday_cpuid_detect(); read-only afterwards.
extern monday_cpu_features_t g_cpu_features;

// ── Matmul dispatch ───────────────────────────────────────────────────────────
//
// Integer matrix-vector multiply kernel used by the inference backends.
// a[m][k] * b[k] → c[m]
// All inputs and outputs are int32_t.  Accumulation is in int64_t internally.
//
// Three compiled variants:
//   matmul_avx2   — AVX2 256-bit SIMD (8×int32 per cycle)
//   matmul_sse4   — SSE4.2 128-bit SIMD (4×int32 per cycle)
//   matmul_scalar — portable scalar reference
//
// The dispatch pointer is set to the correct variant by monday_cpuid_dispatch_init().
typedef void (*monday_matmul_fn_t)(const int32_t* a,
                                    const int32_t* b,
                                    int32_t*       c,
                                    uint32_t       m,
                                    uint32_t       k);

// Globally dispatched matmul function.  Call this — never call variants directly.
extern monday_matmul_fn_t g_monday_matmul;

// Initialise the dispatch table.  Must be called before any inference.
// Also calls monday_cpuid_detect() and stores result in g_cpu_features.
void monday_cpuid_dispatch_init(void);

// Individual kernel entry points (exposed for testing only).
void matmul_avx2  (const int32_t* a, const int32_t* b, int32_t* c, uint32_t m, uint32_t k);
void matmul_sse4  (const int32_t* a, const int32_t* b, int32_t* c, uint32_t m, uint32_t k);
void matmul_scalar(const int32_t* a, const int32_t* b, int32_t* c, uint32_t m, uint32_t k);

#ifdef __cplusplus
}
#endif
