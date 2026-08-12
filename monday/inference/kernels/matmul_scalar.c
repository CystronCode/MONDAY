// Scalar reference: a[m][k] * b[k] -> c[m], int32_t, accumulate in int64_t
#include "monday/inference/cpuid_dispatch.h"
#include <stdint.h>

void matmul_scalar(const int32_t *a, const int32_t *b, int32_t *c,
                   uint32_t m, uint32_t k) {
    for (uint32_t i = 0; i < m; i++) {
        int64_t acc = 0;
        const int32_t *row = a + (size_t)i * k;
        for (uint32_t j = 0; j < k; j++) {
            acc += (int64_t)row[j] * (int64_t)b[j];
        }
        // Clamp to int32_t range
        if (acc > INT32_MAX) acc = INT32_MAX;
        if (acc < INT32_MIN) acc = INT32_MIN;
        c[i] = (int32_t)acc;
    }
}
