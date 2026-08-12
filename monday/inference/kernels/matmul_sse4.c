// SSE4.2 kernel: a[m][k] * b[k] -> c[m], int32_t
// Compiled with -msse4.2
#include "monday/inference/cpuid_dispatch.h"
#include <stdint.h>

#if defined(__SSE4_2__) || defined(__SSE4_1__) || defined(__SSE2__)
#include <emmintrin.h>  // SSE2 _mm_* intrinsics (available with SSE4.2)
#include <smmintrin.h>  // SSE4.1 for _mm_mullo_epi32

void matmul_sse4(const int32_t *a, const int32_t *b, int32_t *c,
                 uint32_t m, uint32_t k) {
    for (uint32_t i = 0; i < m; i++) {
        const int32_t *row = a + (size_t)i * k;
        int64_t acc = 0;
        uint32_t j = 0;
        // Process 4 elements at a time using SSE4.1 _mm_mullo_epi32
        for (; j + 4 <= k; j += 4) {
            __m128i va = _mm_loadu_si128((const __m128i *)(row + j));
            __m128i vb = _mm_loadu_si128((const __m128i *)(b + j));
            __m128i prod = _mm_mullo_epi32(va, vb); // SSE4.1
            // Extract and accumulate
            acc += (int64_t)_mm_extract_epi32(prod, 0);
            acc += (int64_t)_mm_extract_epi32(prod, 1);
            acc += (int64_t)_mm_extract_epi32(prod, 2);
            acc += (int64_t)_mm_extract_epi32(prod, 3);
        }
        // Scalar tail
        for (; j < k; j++) acc += (int64_t)row[j] * (int64_t)b[j];
        if (acc > INT32_MAX) acc = INT32_MAX;
        if (acc < INT32_MIN) acc = INT32_MIN;
        c[i] = (int32_t)acc;
    }
}

#else
// Fallback: no SSE4 available at compile time, use scalar
void matmul_sse4(const int32_t *a, const int32_t *b, int32_t *c,
                 uint32_t m, uint32_t k) {
    for (uint32_t i = 0; i < m; i++) {
        int64_t acc = 0;
        const int32_t *row = a + (size_t)i * k;
        for (uint32_t j = 0; j < k; j++) acc += (int64_t)row[j] * (int64_t)b[j];
        if (acc > INT32_MAX) acc = INT32_MAX;
        if (acc < INT32_MIN) acc = INT32_MIN;
        c[i] = (int32_t)acc;
    }
}
#endif
