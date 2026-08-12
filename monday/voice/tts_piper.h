// monday/voice/tts_piper.h
//
// Piper TTS engine wrapper supporting sentence-boundary token streaming.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "monday/core/errors.h"
#include "monday/inference/onnx_session.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_piper_tts monday_piper_tts_t;

typedef void (*monday_tts_audio_callback_t)(const int16_t *pcm_samples, size_t count, void *user_data);

/* Initialize Piper TTS engine with ONNX model & voice config. */
monday_error_t monday_piper_tts_init(monday_piper_tts_t **out_tts,
                                     monday_onnx_env_t *onnx_env,
                                     const char *model_path,
                                     const char *config_path);

/* Synthesize text chunk to audio PCM samples. */
monday_error_t monday_piper_tts_synthesize(monday_piper_tts_t *tts,
                                           const char *text,
                                           monday_tts_audio_callback_t on_audio,
                                           void *user_data);

/* Stream generated LLM token piece to sentence boundary detector. */
monday_error_t monday_piper_tts_push_token(monday_piper_tts_t *tts,
                                            const char *token_piece,
                                            monday_tts_audio_callback_t on_audio,
                                            void *user_data);

/* Destroy Piper TTS instance. */
void monday_piper_tts_destroy(monday_piper_tts_t *tts);

#ifdef __cplusplus
}
#endif
