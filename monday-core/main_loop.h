// monday-core/main_loop.h
//
// Main inference loop — turn_begin → inference → turn_end skeleton.
//
// Architecture §3.5 / §4 Data Flow: every user interaction flows through:
//   monday_blackboard_turn_begin()
//     → GOAP planner (Phase 2)
//     → BT engine (Phase 2)
//     → LLM inference (this phase)
//     → Action dispatcher (Phase 4)
//     → TTS synthesis (Phase 2)
//   monday_blackboard_turn_end()
//
// Phase 1 implements the LLM inference step only.  The surrounding steps are
// stubbed with no-ops so the terminal chat loop compiles and runs.

#pragma once

#include <stdbool.h>
#include "monday/inference/IInferenceEngine.h"

#ifdef __cplusplus

// Configuration for the main inference loop (loaded from config.toml).
struct MondayLoopConfig {
    float    temperature;       // [inference] temperature
    float    top_p;             // [inference] top_p
    uint32_t max_new_tokens;    // [inference] max_new_tokens
    bool     text_only;         // --text-only CLI flag: suppress voice init
    bool     benchmark_mode;    // --benchmark-inference: extra timing output
};

// Initialise the main loop.  Must be called after the engine is loaded.
// engine  = loaded IInferenceEngine (not null, not stub-only in prod).
// cfg     = loop configuration from config.toml + CLI flags.
void monday_loop_init(monday::IInferenceEngine* engine,
                      const MondayLoopConfig&   cfg);

// Run the terminal chat loop.  Blocks until EOF on stdin or SIGTERM.
// Reads one line from stdin, calls run_inference(), prints the response to stdout.
// Sends heartbeat responses to the supervisor between turns.
// Returns when the loop exits cleanly.
void monday_loop_run(void);

// Tear down the main loop (called from monday_shutdown_clean).
void monday_loop_shutdown(void);

extern "C" {
#endif

// C-linkage stub accessors used by shutdown.c (which is C11).
void monday_loop_shutdown_c(void);

#ifdef __cplusplus
}
#endif
