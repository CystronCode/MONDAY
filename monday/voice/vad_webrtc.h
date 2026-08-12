// monday/voice/vad_webrtc.h
//
// WebRTC VAD Stage 1 low-power idle voice activity detector.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_webrtc_vad monday_webrtc_vad_t;

/* Initialize WebRTC VAD (mode: 0=quality, 1=low-bitrate, 2=aggressive, 3=very-aggressive). */
monday_error_t monday_webrtc_vad_init(monday_webrtc_vad_t **out_vad, int mode);

/* Process 10ms, 20ms, or 30ms PCM frame at 16 kHz. Returns true if speech detected. */
bool monday_webrtc_vad_process(monday_webrtc_vad_t *vad,
                              const int16_t *samples,
                              size_t sample_count,
                              uint32_t sample_rate);

/* Destroy WebRTC VAD instance. */
void monday_webrtc_vad_destroy(monday_webrtc_vad_t *vad);

#ifdef __cplusplus
}
#endif
