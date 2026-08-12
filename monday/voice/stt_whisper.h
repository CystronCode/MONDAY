// monday/voice/stt_whisper.h
//
// whisper.cpp Speech-to-Text (STT) engine wrapper (base.en INT8 / tiny.en).

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "monday/core/errors.h"
#include "monday/core/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct monday_whisper_stt monday_whisper_stt_t;

/* Initialize whisper.cpp STT engine with model file path. */
monday_error_t monday_whisper_stt_init(monday_whisper_stt_t **out_stt, const char *model_path);

/* Process PCM float audio buffer (16 kHz mono) and transcribe text. */
monday_error_t monday_whisper_stt_transcribe(monday_whisper_stt_t *stt,
                                              const float *pcm_samples,
                                              size_t sample_count,
                                              char *out_text_buf,
                                              size_t text_buf_size);

/* Destroy whisper.cpp STT instance. */
void monday_whisper_stt_destroy(monday_whisper_stt_t *stt);

#ifdef __cplusplus
}
#endif
