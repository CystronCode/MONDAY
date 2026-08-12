// monday/inference/engine_selector.h
//
// CPUID-aware backend selector.
//
// Architecture §2.2 / §2.3: selects BitNet (primary) or llama.cpp (fallback)
// based on CPUID capabilities, available RAM, and config.toml settings.
// Returns an owning pointer to the selected IInferenceEngine.

#pragma once

#include <memory>
#include "monday/inference/IInferenceEngine.h"

namespace monday {

// Engine preference hint from config.toml [inference].engine.
enum class EnginePreference {
    AUTO,    // select automatically based on CPUID and RAM
    BITNET,  // prefer BitNet; fall back to llama.cpp on incapability
    LLAMA,   // prefer llama.cpp (use for constrained hardware or testing)
};

// Select and instantiate the appropriate inference backend.
//
// preference   — from config.toml; AUTO is recommended.
// free_ram_mb  — current free physical RAM in megabytes.
//                If < 2048 MB, the selector degrades to llama.cpp with
//                Qwen2.5-0.5B (GRACEFUL_DEGRADED flag set).
//
// Returns owning unique_ptr to a concrete IInferenceEngine implementation.
// The returned engine has NOT been loaded yet — caller must call load_model().
std::unique_ptr<IInferenceEngine> monday_select_engine(
    EnginePreference preference,
    uint64_t         free_ram_mb);

} // namespace monday
