// monday/voice/bandpass_filter.c
//
// Biquad IIR bandpass filter (80 Hz - 8 kHz) implementation.

#include "monday/voice/bandpass_filter.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void init_highpass_stage(monday_biquad_stage_t *stage, double fc, double fs) {
    double omega = 2.0 * M_PI * fc / fs;
    double sn = sin(omega);
    double cs = cos(omega);
    double alpha = sn / (2.0 * 0.7071067811865475); /* Q = 1/sqrt(2) */

    double b0 =  (1.0 + cs) / 2.0;
    double b1 = -(1.0 + cs);
    double b2 =  (1.0 + cs) / 2.0;
    double a0 =   1.0 + alpha;
    double a1 =  -2.0 * cs;
    double a2 =   1.0 - alpha;

    stage->b0 = b0 / a0;
    stage->b1 = b1 / a0;
    stage->b2 = b2 / a0;
    stage->a1 = a1 / a0;
    stage->a2 = a2 / a0;
    stage->x1 = stage->x2 = stage->y1 = stage->y2 = 0.0;
}

static void init_lowpass_stage(monday_biquad_stage_t *stage, double fc, double fs, double q) {
    double omega = 2.0 * M_PI * fc / fs;
    double sn = sin(omega);
    double cs = cos(omega);
    double alpha = sn / (2.0 * q);

    double b0 =  (1.0 - cs) / 2.0;
    double b1 =   1.0 - cs;
    double b2 =  (1.0 - cs) / 2.0;
    double a0 =   1.0 + alpha;
    double a1 =  -2.0 * cs;
    double a2 =   1.0 - alpha;

    stage->b0 = b0 / a0;
    stage->b1 = b1 / a0;
    stage->b2 = b2 / a0;
    stage->a1 = a1 / a0;
    stage->a2 = a2 / a0;
    stage->x1 = stage->x2 = stage->y1 = stage->y2 = 0.0;
}

static inline double biquad_step(monday_biquad_stage_t *s, double x) {
    double y = s->b0 * x + s->b1 * s->x1 + s->b2 * s->x2 - s->a1 * s->y1 - s->a2 * s->y2;
    s->x2 = s->x1;
    s->x1 = x;
    s->y2 = s->y1;
    s->y1 = y;
    return y;
}

monday_error_t monday_bandpass_filter_init(monday_bandpass_filter_t *filter, uint32_t sample_rate) {
    if (!filter || sample_rate == 0) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null argument to bandpass filter init");
    }

    filter->sample_rate = sample_rate;
    init_highpass_stage(&filter->highpass, 80.0, (double)sample_rate);
    /* 4th order Butterworth lowpass using 2 cascaded biquads with Q1=0.541, Q2=1.306 */
    init_lowpass_stage(&filter->lowpass1, 7900.0, (double)sample_rate, 0.541196100146197);
    init_lowpass_stage(&filter->lowpass2, 7900.0, (double)sample_rate, 1.3065629648763765);

    return MONDAY_ERROR(MONDAY_OK, "bandpass filter initialized");
}

int16_t monday_bandpass_filter_process_sample(monday_bandpass_filter_t *filter, int16_t input) {
    if (!filter) return input;

    double x = (double)input;
    double hp_out = biquad_step(&filter->highpass, x);
    double lp1_out = biquad_step(&filter->lowpass1, hp_out);
    double lp2_out = biquad_step(&filter->lowpass2, lp1_out);

    if (lp2_out > 32767.0) return 32767;
    if (lp2_out < -32768.0) return -32768;
    return (int16_t)lp2_out;
}

void monday_bandpass_filter_process_buffer(monday_bandpass_filter_t *filter, int16_t *samples, size_t count) {
    if (!filter || !samples) return;
    for (size_t i = 0; i < count; i++) {
        samples[i] = monday_bandpass_filter_process_sample(filter, samples[i]);
    }
}
