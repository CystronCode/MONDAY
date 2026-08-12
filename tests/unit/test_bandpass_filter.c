// tests/unit/test_bandpass_filter.c
//
// Unit tests for biquad IIR 80 Hz – 8 kHz bandpass filter (including 19 kHz DolphinAttack defense).

#include "unity.h"
#include "monday/voice/bandpass_filter.h"
#include "monday/core/allocator.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void setUp(void) {
    monday_allocator_init(4*1024*1024, 8*1024*1024, 4*1024*1024, 8*1024*1024);
}

void tearDown(void) {}

static double compute_attenuation_db(double freq_hz, uint32_t sample_rate) {
    monday_bandpass_filter_t filter;
    monday_bandpass_filter_init(&filter, sample_rate);

    /* Generate 2000 samples of sine wave */
    double in_power = 0.0;
    double out_power = 0.0;

    for (size_t i = 0; i < 2000; i++) {
        double t = (double)i / (double)sample_rate;
        double s = sin(2.0 * M_PI * freq_hz * t) * 20000.0;
        int16_t in_sample = (int16_t)s;
        int16_t out_sample = monday_bandpass_filter_process_sample(&filter, in_sample);

        if (i >= 500) { /* Ignore initial transient response */
            in_power += (double)in_sample * (double)in_sample;
            out_power += (double)out_sample * (double)out_sample;
        }
    }

    if (out_power <= 1e-9) return 100.0;
    double ratio = out_power / in_power;
    return -10.0 * log10(ratio);
}

void test_bandpass_passband_1khz(void) {
    double atten = compute_attenuation_db(1000.0, 16000);
    TEST_ASSERT_TRUE_MESSAGE(atten < 3.0, "1 kHz passband attenuation must be < 3 dB");
}

void test_bandpass_ultrasonic_19khz(void) {
    double atten = compute_attenuation_db(19000.0, 48000);
    TEST_ASSERT_TRUE_MESSAGE(atten > 50.0, "19 kHz ultrasonic attenuation must be > 50 dB");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bandpass_passband_1khz);
    RUN_TEST(test_bandpass_ultrasonic_19khz);
    return UNITY_END();
}
