#include "monday/inference/cpuid_dispatch.h"
#include <stdint.h>

monday_cpu_features_t g_cpu_features = MONDAY_CPU_SCALAR;
monday_matmul_fn_t g_monday_matmul = NULL;

monday_cpu_features_t monday_cpuid_detect(void) {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2")) {
        g_cpu_features = MONDAY_CPU_AVX2;
    } else if (__builtin_cpu_supports("sse4.2")) {
        g_cpu_features = MONDAY_CPU_SSE4_2;
    } else {
        g_cpu_features = MONDAY_CPU_SCALAR;
    }
#else
    g_cpu_features = MONDAY_CPU_SCALAR;
#endif
    return g_cpu_features;
}

void monday_cpuid_dispatch_init(void) {
    monday_cpuid_detect();
    switch (g_cpu_features) {
        case MONDAY_CPU_AVX2:   g_monday_matmul = matmul_avx2;   break;
        case MONDAY_CPU_SSE4_2: g_monday_matmul = matmul_sse4;   break;
        default:                g_monday_matmul = matmul_scalar; break;
    }
}
