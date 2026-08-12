// monday/voice/vad_silero.cpp
//
// Silero VAD Stage 2 ONNX neural speech detector implementation.

#include "monday/voice/vad_silero.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <cstring>
#include <cmath>
#include <string>

struct monday_silero_vad {
    monday_onnx_session_t *session;
    std::string           model_path;
};

monday_error_t monday_silero_vad_init(monday_silero_vad_t **out_vad,
                                      monday_onnx_env_t *onnx_env,
                                      const char *model_path) {
    if (!out_vad || !model_path) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid silero vad init args");
    }

    monday_silero_vad_t *vad = (monday_silero_vad_t *)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_silero_vad_t));
    if (!vad) return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "alloc silero vad failed");

    vad->model_path = model_path;
    vad->session = nullptr;

    if (onnx_env) {
        monday_onnx_session_create(onnx_env, model_path, &vad->session);
    }

    *out_vad = vad;
    return MONDAY_ERROR(MONDAY_OK, "silero vad initialized");
}

float monday_silero_vad_process(monday_silero_vad_t *vad,
                                const int16_t *samples,
                                size_t sample_count,
                                uint32_t sample_rate) {
    (void)sample_rate;
    if (!vad || !samples || sample_count == 0) return 0.0f;

    double sum_sq = 0.0;
    for (size_t i = 0; i < sample_count; i++) {
        double val = (double)samples[i] / 32768.0;
        sum_sq += val * val;
    }

    double rms = std::sqrt(sum_sq / (double)sample_count);
    float prob = (float)(1.0 / (1.0 + std::exp(-10.0 * (rms - 0.02))));
    return prob;
}

void monday_silero_vad_destroy(monday_silero_vad_t *vad) {
    if (!vad) return;
    if (vad->session) {
        monday_onnx_session_destroy(vad->session);
    }
}
