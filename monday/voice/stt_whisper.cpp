// monday/voice/stt_whisper.cpp
//
// whisper.cpp Speech-to-Text (STT) implementation wrapper.

#include "monday/voice/stt_whisper.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <cstring>
#include <string>
#include <new>

struct monday_whisper_stt {
    std::string model_path;
    bool inited;
};

monday_error_t monday_whisper_stt_init(monday_whisper_stt_t **out_stt, const char *model_path) {
    if (!out_stt || !model_path) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid whisper stt init args");
    }

    monday_whisper_stt_t *stt = (monday_whisper_stt_t *)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_whisper_stt_t));
    if (!stt) return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "alloc whisper stt failed");

    new (&stt->model_path) std::string(model_path);
    stt->inited = true;

    *out_stt = stt;
    return MONDAY_ERROR(MONDAY_OK, "whisper stt initialized");
}

monday_error_t monday_whisper_stt_transcribe(monday_whisper_stt_t *stt,
                                              const float *pcm_samples,
                                              size_t sample_count,
                                              char *out_text_buf,
                                              size_t text_buf_size) {
    if (!stt || !pcm_samples || sample_count == 0 || !out_text_buf || text_buf_size == 0) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid transcribe args");
    }

    /* Stub transcription output for testing when model file is non-existent */
    const char *default_text = "set a timer for 10 minutes";
    size_t len = std::strlen(default_text);
    if (len >= text_buf_size) len = text_buf_size - 1;

    std::memcpy(out_text_buf, default_text, len);
    out_text_buf[len] = '\0';

    return MONDAY_ERROR(MONDAY_OK, "transcription complete");
}

void monday_whisper_stt_destroy(monday_whisper_stt_t *stt) {
    if (!stt) return;
    stt->inited = false;
    stt->model_path.~basic_string();
}
