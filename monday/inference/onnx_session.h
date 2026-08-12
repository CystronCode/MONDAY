// monday/inference/onnx_session.h
//
// Shared ONNX Runtime environment manager (Architecture §3.1 / 3.2).
// Piper TTS, Silero VAD, and MiniLM embedding models share a single ONNX environment.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "monday/core/errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_onnx_env monday_onnx_env_t;
typedef struct monday_onnx_session monday_onnx_session_t;

/* Initialize shared ONNX Runtime environment. */
monday_error_t monday_onnx_env_init(monday_onnx_env_t **out_env);

/* Create an ONNX session from model file path using shared environment. */
monday_error_t monday_onnx_session_create(monday_onnx_env_t *env,
                                          const char *model_path,
                                          monday_onnx_session_t **out_session);

/* Destroy an ONNX session. */
void monday_onnx_session_destroy(monday_onnx_session_t *session);

/* Shutdown shared ONNX Runtime environment. */
void monday_onnx_env_shutdown(monday_onnx_env_t *env);

#ifdef __cplusplus
}
#endif
