// AVX2 kernel: a[m][k] * b[k] -> c[m], int32_t
// Compiled with -mavx2
#include "monday/inference/cpuid_dispatch.h"
#include <stdint.h>

#if defined(__AVX2__)
#include <immintrin.h>  // AVX2 intrinsics

void matmul_avx2(const int32_t *a, const int32_t *b, int32_t *c,
                 uint32_t m, uint32_t k) {
    for (uint32_t i = 0; i < m; i++) {
        const int32_t *row = a + (size_t)i * k;
        int64_t acc = 0;
        uint32_t j = 0;
        // Process 8 elements at a time with AVX2 _mm256_mullo_epi32
        for (; j + 8 <= k; j += 8) {
            __m256i va = _mm256_loadu_si256((const __m256i *)(row + j));
            __m256i vb = _mm256_loadu_si256((const __m256i *)(b + j));
            __m256i prod = _mm256_mullo_epi32(va, vb);
            // Horizontal add via _mm256_hadd and extract
            __m128i lo = _mm256_castsi256_si128(prod);
            __m128i hi = _mm256_extracti128_si256(prod, 1);
            __m128i sum128 = _mm_add_epi32(lo, hi);
            // hadd twice to get 4->2->1
            sum128 = _mm_hadd_epi32(sum128, sum128);
            sum128 = _mm_hadd_epi32(sum128, sum128);
            acc += (int64_t)_mm_cvtsi128_si32(sum128);
        }
        // Scalar tail
        for (; j < k; j++) acc += (int64_t)row[j] * (int64_t)b[j];
        if (acc > INT32_MAX) acc = INT32_MAX;
        if (acc < INT32_MIN) acc = INT32_MIN;
        c[i] = (int32_t)acc;
    }
}

#else
void matmul_avx2(const int32_t *a, const int32_t *b, int32_t *c,
                 uint32_t m, uint32_t k) {
    // Fallback: compiled without AVX2 support
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
