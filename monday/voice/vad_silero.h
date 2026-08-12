// monday/voice/vad_silero.h
//
// Silero VAD Stage 2 ONNX neural speech detector (~8 MB RSS).

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "monday/core/errors.h"
#include "monday/inference/onnx_session.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_silero_vad monday_silero_vad_t;

/* Initialize Silero VAD ONNX model session. */
monday_error_t monday_silero_vad_init(monday_silero_vad_t **out_vad,
                                      monday_onnx_env_t *onnx_env,
                                      const char *model_path);

/* Evaluate candidate speech window. Returns confidence score in range [0.0, 1.0]. */
float monday_silero_vad_process(monday_silero_vad_t *vad,
                                const int16_t *samples,
                                size_t sample_count,
                                uint32_t sample_rate);

/* Destroy Silero VAD session. */
void monday_silero_vad_destroy(monday_silero_vad_t *vad);

#ifdef __cplusplus
}
#endif
