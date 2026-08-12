// monday/voice/vad_webrtc.cpp
//
// WebRTC VAD Stage 1 low-power idle detector implementation.

#include "monday/voice/vad_webrtc.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <cstring>
#include <cmath>

struct monday_webrtc_vad {
    int mode;
};

monday_error_t monday_webrtc_vad_init(monday_webrtc_vad_t **out_vad, int mode) {
    if (!out_vad) return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null out_vad");

    monday_webrtc_vad_t *vad = (monday_webrtc_vad_t *)monday_zone_alloc(MONDAY_ZONE_STATIC, sizeof(monday_webrtc_vad_t));
    if (!vad) return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "alloc webrtc vad failed");

    vad->mode = mode;
    *out_vad = vad;
    return MONDAY_ERROR(MONDAY_OK, "webrtc vad initialized");
}

bool monday_webrtc_vad_process(monday_webrtc_vad_t *vad,
                              const int16_t *samples,
                              size_t sample_count,
                              uint32_t sample_rate) {
    (void)sample_rate;
    if (!vad || !samples || sample_count == 0) return false;

    /* Compute energy threshold for energy-based VAD baseline */
    double sum_sq = 0.0;
    for (size_t i = 0; i < sample_count; i++) {
        double val = (double)samples[i];
        sum_sq += val * val;
    }

    double rms = std::sqrt(sum_sq / (double)sample_count);
    return rms > 300.0; /* Energy threshold for speech detection */
}

void monday_webrtc_vad_destroy(monday_webrtc_vad_t *vad) {
    if (!vad) return;
    vad->mode = 0;
}
