// monday/voice/bandpass_filter.h
//
// 80 Hz – 8 kHz biquad IIR bandpass filter for PCM audio streams (16 kHz / 48 kHz sampling rate).
// Cascaded 4th-order lowpass provides >60 dB attenuation at 19 kHz (DolphinAttack defense).

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_biquad_stage {
    double b0, b1, b2;
    double a1, a2;
    double x1, x2;
    double y1, y2;
} monday_biquad_stage_t;

typedef struct monday_bandpass_filter {
    monday_biquad_stage_t highpass;   /* 80 Hz cutoff highpass stage */
    monday_biquad_stage_t lowpass1;    /* 7900 Hz cutoff lowpass stage 1 */
    monday_biquad_stage_t lowpass2;    /* 7900 Hz cutoff lowpass stage 2 */
    uint32_t sample_rate;
} monday_bandpass_filter_t;

/* Initialize 80 Hz – 8 kHz bandpass filter for a given sample rate (default 16000 Hz / 48000 Hz). */
monday_error_t monday_bandpass_filter_init(monday_bandpass_filter_t *filter, uint32_t sample_rate);

/* Process a single int16_t PCM sample. Returns filtered sample clamped to int16 range. */
int16_t monday_bandpass_filter_process_sample(monday_bandpass_filter_t *filter, int16_t input);

/* Process a buffer of int16_t PCM samples in-place. */
void monday_bandpass_filter_process_buffer(monday_bandpass_filter_t *filter, int16_t *samples, size_t count);

#ifdef __cplusplus
}
#endif
