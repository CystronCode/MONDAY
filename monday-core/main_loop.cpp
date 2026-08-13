#include "monday-core/main_loop.h"
#include "monday-core/shutdown.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"
#include "monday/inference/IInferenceEngine.h"
#include "monday/voice/audio_io.h"
#include "monday/voice/vad_webrtc.h"
#include "monday/voice/stt_whisper.h"
#include "monday/goap/goap_planner.h"
#include "monday/automation/tool_dispatcher.h"
#include "src/generated/blackboard_accessors.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

using namespace monday;

// ── Internal state ────────────────────────────────────────────────────────────

static IInferenceEngine* s_engine     = nullptr;
static MondayLoopConfig  s_cfg        = {};
static bool              s_running    = false;
static std::string       s_system_prompt;
static GOAPPlanner       s_goap_planner;
static ToolDispatcher    s_tool_dispatcher(nullptr, true);

// ── Turn lifecycle ────────────────────────────────────────────────────────────

static void turn_begin_stub(void) {
    // Phase 1: reset PU_LEVEL arena
    monday_level_reset();
}

static void turn_end_stub(void) {
}

// ── System Prompt Loader ──────────────────────────────────────────────────────

static std::string load_system_prompt() {
    const char* paths[] = {
        "data/prompts/system_prompt.txt",
        "../data/prompts/system_prompt.txt",
        "monday/data/prompts/system_prompt.txt",
        "\\\\wsl.localhost\\Ubuntu-26.04\\home\\cystron_code\\monday\\data\\prompts\\system_prompt.txt"
    };

    for (const char* p : paths) {
        std::ifstream file(p);
        if (file.is_open()) {
            std::stringstream ss;
            ss << file.rdbuf();
            monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Loaded system prompt from file");
            return ss.str();
        }
    }

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Using fallback internal system prompt");
    return "You are Monday, a fully offline desktop AI assistant. Be concise, direct, and clear.";
}

// ── Turn Processing Pipeline (GOAP -> ToolDispatcher -> LLM Fallback) ───────

static void process_user_turn(const std::string& input_text) {
    if (input_text.empty()) return;

    turn_begin_stub();

    // 1. GOAP Intent Classification (< 5 ms execution target)
    GOAPResult goap_res = s_goap_planner.classify(input_text.c_str());

    if (goap_res.matched && goap_res.confidence >= 0.85f) {
        // Fast path: Execute tool directly, bypassing LLM per Architecture §2.5 & §4 Step 3
        fprintf(stdout, "[GOAP Fast-Path Match: %s (conf: %.2f)]\n", goap_res.intent_id.c_str(), goap_res.confidence);
        fflush(stdout);

        ToolCall call = {};
        call.tool_name = goap_res.action;
        call.action_class = goap_res.action_class;
        call.target_scope = goap_res.param_name;
        call.description = "Executed via GOAP intent fast-path";

        if (!goap_res.param_str_val.empty()) {
            call.parameters_json = goap_res.param_str_val;
        } else if (goap_res.param_int_val != 0) {
            call.parameters_json = std::to_string(goap_res.param_int_val);
        } else {
            // Check if action parameter relates to app name extraction
            call.parameters_json = input_text;
        }

        auto exec_res = s_tool_dispatcher.execute_tool(call);
        if (exec_res.has_value()) {
            if (exec_res.value().success) {
                fprintf(stdout, "Monday: %s\n", exec_res.value().error_message.c_str());
            } else {
                fprintf(stdout, "Monday: [Execution Error] %s\n", exec_res.value().error_message.c_str());
            }
        } else {
            fprintf(stdout, "Monday: [Dispatch Failed] %s\n", exec_res.error().message);
        }
        fflush(stdout);
        turn_end_stub();
        return;
    }

    // 2. LLM Fallback path when GOAP confidence < 0.85
    std::string formatted_prompt = "<|im_start|>system\n" + s_system_prompt + "\n<|im_end|>\n<|im_start|>user\n" + input_text + "<|im_end|>\n<|im_start|>assistant\n";

    InferenceRequest req = {};
    req.prompt         = formatted_prompt.c_str();
    req.max_new_tokens = s_cfg.max_new_tokens ? s_cfg.max_new_tokens : 512;
    req.temperature    = s_cfg.temperature;
    req.top_p          = s_cfg.top_p;
    req.append_to_kv   = true;

    fprintf(stdout, "Monday: ");
    fflush(stdout);

    req.on_token = [](int32_t tok_id, const char *tok_text) -> bool {
        if (tok_id == -1 || !tok_text) return false;
        fputs(tok_text, stdout);
        fflush(stdout);
        return true;
    };

    auto result = s_engine->run_inference(req);
    fprintf(stdout, "\n");
    fflush(stdout);

    if (!result) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "main_loop: inference error");
        fprintf(stdout, "[Error: inference failed]\n");
    }

    turn_end_stub();

    if (s_cfg.benchmark_mode && result) {
        fprintf(stdout, "[tokens=%u, truncated=%s]\n",
                result->token_count,
                result->was_truncated ? "yes" : "no");
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void monday_loop_init(monday::IInferenceEngine *engine,
                      const MondayLoopConfig   &cfg) {
    s_engine  = engine;
    s_cfg     = cfg;
    s_running = false;

    s_system_prompt = load_system_prompt();

    // Initialize GOAP planner
    const char* intent_paths[] = {
        "data/intents.jsonl",
        "../data/intents.jsonl",
        "monday/data/intents.jsonl",
        "\\\\wsl.localhost\\Ubuntu-26.04\\home\\cystron_code\\monday\\data\\intents.jsonl"
    };
    bool goap_init_ok = false;
    for (const char* ipath : intent_paths) {
        if (s_goap_planner.init(ipath).has_value()) {
            goap_init_ok = true;
            monday_log_write(MONDAY_LOG_DIAGNOSTIC, "GOAP planner initialized successfully");
            break;
        }
    }
    if (!goap_init_ok) {
        monday_log_write(MONDAY_LOG_SYSTEM, "main_loop: failed to load GOAP intents file");
    }
}

void monday_loop_run(void) {
    if (!s_engine) {
        monday_log_write(MONDAY_LOG_SYSTEM, "main_loop: no engine — cannot run");
        return;
    }

    s_running = true;
    char line_buf[8192];

    // ── Wire PortAudio Microphone Stream ──────────────────────────────────────────
    monday_audio_stream_t *audio_stream = NULL;
    monday_audio_config_t audio_cfg = {16000, 1, 512, 8192, true};
    monday_error_t audio_err = monday_audio_init(&audio_stream, &audio_cfg);
    bool voice_active = false;
    std::thread voice_thread;

    if (audio_err.code == MONDAY_OK && audio_stream) {
        if (monday_audio_start(audio_stream).code == MONDAY_OK) {
            voice_active = true;
            monday_log_write(MONDAY_LOG_DIAGNOSTIC, "PortAudio microphone capture initialized and recording at 16kHz");

            voice_thread = std::thread([audio_stream]() {
                monday_webrtc_vad_t *vad = nullptr;
                monday_webrtc_vad_init(&vad, 2);
                monday_whisper_stt_t *stt = nullptr;
                monday_whisper_stt_init(&stt, "whisper-base.en.bin");

                int16_t pcm_frame[512];
                bool in_speech = false;
                uint32_t speech_count = 0;
                std::vector<float> speech_buffer;

                while (s_running) {
                    if (monday_audio_read(audio_stream, pcm_frame, 512).code == MONDAY_OK) {
                        bool is_speech = monday_webrtc_vad_process(vad, pcm_frame, 512, 16000);
                        if (is_speech) {
                            speech_count++;
                            if (speech_count == 3 && !in_speech) {
                                in_speech = true;
                                speech_buffer.clear();
                                fprintf(stdout, "\n[Voice Activity Detected! Listening...]\n");
                                fflush(stdout);
                            }
                            if (in_speech) {
                                for (int i = 0; i < 512; i++) {
                                    speech_buffer.push_back(pcm_frame[i] / 32768.0f);
                                }
                            }
                        } else {
                            if (in_speech && speech_count > 5) {
                                fprintf(stdout, "[Voice Pause Detected — Transcribing & Processing...]\n");
                                fflush(stdout);

                                char voice_prompt[1024] = {0};
                                if (!speech_buffer.empty()) {
                                    monday_whisper_stt_transcribe(stt, speech_buffer.data(), speech_buffer.size(), voice_prompt, sizeof(voice_prompt));
                                }

                                if (strlen(voice_prompt) > 0) {
                                    fprintf(stdout, "\nVoice Prompt: %s\n", voice_prompt);
                                    fflush(stdout);

                                    // Run voice prompt through the exact same GOAP -> ToolDispatcher -> LLM pipeline
                                    process_user_turn(voice_prompt);

                                    fprintf(stdout, "\nYou: ");
                                    fflush(stdout);
                                }
                            }
                            in_speech = false;
                            speech_count = 0;
                            speech_buffer.clear();
                        }
                    } else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
                if (vad) monday_webrtc_vad_destroy(vad);
                if (stt) monday_whisper_stt_destroy(stt);
            });
        }
    }

    // Prompt to user
    fprintf(stdout, "\n===================================================\n");
    fprintf(stdout, "  Monday AI Assistant Ready (AVX2 Hardware Engine)\n");
    if (voice_active) {
        fprintf(stdout, "  [Voice Active]: Real-time WebRTC VAD Mic Monitor\n");
    }
    fprintf(stdout, "  Type a message and press Enter (or 'exit' to quit)\n");
    fprintf(stdout, "===================================================\n\n");

    while (s_running) {
        fprintf(stdout, "You: ");
        fflush(stdout);

        if (!fgets(line_buf, sizeof(line_buf), stdin)) {
            break;
        }

        // Strip trailing newline
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len-1]=='\n'||line_buf[len-1]=='\r')) {
            line_buf[--len] = '\0';
        }
        if (len == 0) continue;
        if (strcmp(line_buf, "exit") == 0 || strcmp(line_buf, "quit") == 0) break;

        process_user_turn(line_buf);
    }

    if (audio_stream) {
        monday_audio_stop(audio_stream);
        monday_audio_shutdown(audio_stream);
    }
    if (voice_thread.joinable()) {
        voice_thread.join();
    }

    s_running = false;
    monday_log_write(MONDAY_LOG_SYSTEM, "main_loop: exiting");
}

void monday_loop_shutdown(void) {
    s_running = false;
}

extern "C" void monday_loop_shutdown_c(void) {
    monday_loop_shutdown();
}
