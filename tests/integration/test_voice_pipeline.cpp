// tests/integration/test_voice_pipeline.cpp
//
// Integration test for Phase 2 Voice Pipeline + GOAP Fast Path.

#include <catch2/catch_test_macros.hpp>

#include "monday/core/allocator.h"
#include "monday/core/spsc_ring.h"
#include "monday/voice/bandpass_filter.h"
#include "monday/voice/vad_webrtc.h"
#include "monday/voice/vad_silero.h"
#include "monday/voice/stt_whisper.h"
#include "monday/voice/tts_piper.h"
#include "monday/goap/goap_planner.h"
#include "monday/bt/bt_engine.h"

using namespace monday;

TEST_CASE("Voice Pipeline: End-to-end VAD to STT to GOAP dispatch", "[voice][integration]") {
    monday_allocator_init(4*1024*1024, 8*1024*1024, 4*1024*1024, 8*1024*1024);

    /* 1. Bandpass filter init */
    monday_bandpass_filter_t filter;
    REQUIRE(monday_bandpass_filter_init(&filter, 16000).code == MONDAY_OK);

    /* 2. WebRTC VAD Stage 1 init */
    monday_webrtc_vad_t *webrtc_vad = nullptr;
    REQUIRE(monday_webrtc_vad_init(&webrtc_vad, 2).code == MONDAY_OK);

    /* 3. Whisper STT init */
    monday_whisper_stt_t *stt = nullptr;
    REQUIRE(monday_whisper_stt_init(&stt, "dummy_model.bin").code == MONDAY_OK);

    /* 4. Transcribe audio */
    float pcm_samples[1600] = {0.5f};
    char text_buf[256] = {0};
    REQUIRE(monday_whisper_stt_transcribe(stt, pcm_samples, 1600, text_buf, sizeof(text_buf)).code == MONDAY_OK);
    REQUIRE(strlen(text_buf) > 0);

    /* 5. GOAP dispatch */
    GOAPPlanner planner;
    REQUIRE(planner.init("data/intents.jsonl").has_value());

    GOAPResult res = planner.classify(text_buf);
    REQUIRE(res.matched == true);
    REQUIRE(res.action == "SYSTEM_SET_TIMER");

    monday_whisper_stt_destroy(stt);
    monday_webrtc_vad_destroy(webrtc_vad);
}
