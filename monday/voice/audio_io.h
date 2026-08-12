// monday/voice/audio_io.h
//
// PortAudio capture abstraction layer. Zero heap allocation on audio thread.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "monday/core/errors.h"
#include "monday/core/spsc_ring.h"
#include "monday/voice/bandpass_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_audio_config {
    uint32_t sample_rate;    /* Default: 16000 Hz */
    uint32_t channels;       /* Default: 1 (mono) */
    uint32_t frame_size;     /* Default: 512 samples per frame */
    size_t   ring_capacity;  /* Must be power of two, e.g., 8192 */
    bool     enable_filter;  /* Enable 80 Hz – 8 kHz bandpass filter */
} monday_audio_config_t;

typedef struct monday_audio_stream monday_audio_stream_t;

/* Initialize PortAudio system and audio capture stream. */
monday_error_t monday_audio_init(monday_audio_stream_t **out_stream,
                                 const monday_audio_config_t *config);

/* Start audio capture. */
monday_error_t monday_audio_start(monday_audio_stream_t *stream);

/* Stop audio capture. */
monday_error_t monday_audio_stop(monday_audio_stream_t *stream);

/* Pop captured PCM frames from SPSC ring buffer (Processing thread). */
monday_error_t monday_audio_read(monday_audio_stream_t *stream,
                                 int16_t *out_samples,
                                 size_t sample_count);

/* Shutdown audio capture stream and PortAudio. */
void monday_audio_shutdown(monday_audio_stream_t *stream);

#ifdef __cplusplus
}
#endif
