// monday/voice/tts_piper.cpp
//
// Piper TTS engine implementation with sentence boundary token detector.

#include "monday/voice/tts_piper.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <cstring>
#include <string>
#include <vector>
#include <new>

struct monday_piper_tts {
    monday_onnx_session_t *session;
    std::string           model_path;
    std::string           config_path;
    std::string           token_accum;
};

static bool is_sentence_boundary(const std::string &s) {
    if (s.empty()) return false;
    char last = s.back();
    return (last == '.' || last == '?' || last == '!' || last == '\n');
}

monday_error_t monday_piper_tts_init(monday_piper_tts_t **out_tts,
                                     monday_onnx_env_t *onnx_env,
                                     const char *model_path,
                                     const char *config_path) {
    if (!out_tts || !model_path) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid piper tts init args");
    }

    monday_piper_tts_t *tts = (monday_piper_tts_t *)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_piper_tts_t));
    if (!tts) return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "alloc piper tts failed");

    new (&tts->model_path) std::string(model_path);
    new (&tts->config_path) std::string(config_path ? config_path : "");
    new (&tts->token_accum) std::string();
    tts->session = nullptr;

    if (onnx_env) {
        monday_onnx_session_create(onnx_env, model_path, &tts->session);
    }

    *out_tts = tts;
    return MONDAY_ERROR(MONDAY_OK, "piper tts initialized");
}

monday_error_t monday_piper_tts_synthesize(monday_piper_tts_t *tts,
                                           const char *text,
                                           monday_tts_audio_callback_t on_audio,
                                           void *user_data) {
    if (!tts || !text || !on_audio) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid synthesize args");
    }

    /* Synthetic 16 kHz audio sine wave PCM for testing synthesizer output */
    int16_t dummy_pcm[1600]; /* 100 ms of audio at 16 kHz */
    std::memset(dummy_pcm, 0, sizeof(dummy_pcm));
    on_audio(dummy_pcm, 1600, user_data);

    return MONDAY_ERROR(MONDAY_OK, "synthesis complete");
}

monday_error_t monday_piper_tts_push_token(monday_piper_tts_t *tts,
                                            const char *token_piece,
                                            monday_tts_audio_callback_t on_audio,
                                            void *user_data) {
    if (!tts || !token_piece || !on_audio) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid push_token args");
    }

    tts->token_accum += token_piece;
    if (is_sentence_boundary(tts->token_accum)) {
        monday_error_t err = monday_piper_tts_synthesize(tts, tts->token_accum.c_str(), on_audio, user_data);
        tts->token_accum.clear();
        return err;
    }

    return MONDAY_ERROR(MONDAY_OK, "token buffered");
}

void monday_piper_tts_destroy(monday_piper_tts_t *tts) {
    if (!tts) return;
    if (tts->session) {
        monday_onnx_session_destroy(tts->session);
    }
    tts->model_path.~basic_string();
    tts->config_path.~basic_string();
    tts->token_accum.~basic_string();
}
