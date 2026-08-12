#include "monday/inference/engine_selector.h"
#include "monday/inference/backends/bitnet_engine.h"
#include "monday/inference/backends/llama_engine.h"
#include "monday/inference/cpuid_dispatch.h"
#include "monday/core/log.h"
#include <memory>

namespace monday {

std::unique_ptr<IInferenceEngine> monday_select_engine(
        EnginePreference preference,
        uint64_t         free_ram_mb) {

    // Architecture §2.3: degrade to llama.cpp on sub-2 GB systems
    const bool ram_constrained = (free_ram_mb < 2048);

    bool use_bitnet = false;
    switch (preference) {
        case EnginePreference::BITNET:
            use_bitnet = !ram_constrained;
            break;
        case EnginePreference::LLAMA:
            use_bitnet = false;
            break;
        case EnginePreference::AUTO:
        default:
            // AVX2 or SSE4.2 + sufficient RAM → BitNet
            use_bitnet = !ram_constrained &&
                         (g_cpu_features == MONDAY_CPU_AVX2 ||
                          g_cpu_features == MONDAY_CPU_SSE4_2);
            break;
    }

    if (use_bitnet) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC,
                         "engine_selector: selecting BitNet b1.58 2B4T");
        return std::make_unique<BitnetEngine>();
    } else {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC,
                         "engine_selector: selecting llama.cpp (GGUF fallback)");
        return std::make_unique<LlamaEngine>();
    }
}

} // namespace monday
