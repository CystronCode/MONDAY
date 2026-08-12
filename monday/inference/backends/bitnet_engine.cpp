// monday/inference/backends/bitnet_engine.cpp
//
// IInferenceEngine stub adapter for BitNet b1.58 2B4T.
// Implements the exact interface defined in IInferenceEngine.h using tl::expected.

#include "monday/inference/backends/bitnet_engine.h"
#include "monday/inference/cpuid_dispatch.h"
#include "monday/security/model_verify.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <string>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <sstream>

namespace monday {

// ── Impl ──────────────────────────────────────────────────────────────────────

struct BitnetEngine::Impl {
    std::string         model_path;
    std::string         manifest_path;
    bool                loaded       = false;
    uint32_t            kv_generation = 0;
    InferenceCapability capabilities  = InferenceCapability::NONE;
};

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────

BitnetEngine::BitnetEngine()  : impl_(new Impl{}) {}
BitnetEngine::~BitnetEngine() { delete impl_; }

// ── load_model ────────────────────────────────────────────────────────────────

Result<void> BitnetEngine::load_model(const char *model_path,
                                       const char *manifest_path) {
    if (!model_path || !manifest_path) {
        return tl::unexpected(
            MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null path argument to load_model"));
    }

    monday_model_manifest_t manifest;
    monday_error_t err = monday_model_verify(manifest_path, &manifest);
    if (err.code != MONDAY_OK) {
        return tl::unexpected(err);
    }

    impl_->model_path    = model_path;
    impl_->manifest_path = manifest_path;
    impl_->loaded        = true;
    impl_->kv_generation = 0;

    // Set capability flags
    impl_->capabilities = InferenceCapability::STREAMING
                        | InferenceCapability::KV_CACHE
                        | InferenceCapability::INT4_QUANTIZED
                        | InferenceCapability::CONTEXT_SLIDING;
    if (g_cpu_features == MONDAY_CPU_AVX2) {
        impl_->capabilities = impl_->capabilities | InferenceCapability::AVX2;
    } else if (g_cpu_features == MONDAY_CPU_SSE4_2) {
        impl_->capabilities = impl_->capabilities | InferenceCapability::SSE4_2;
    }

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "bitnet_engine: model loaded (stub)");
    return {};  // tl::expected<void> success
}

// ── unload_model ──────────────────────────────────────────────────────────────

void BitnetEngine::unload_model() {
    impl_->loaded        = false;
    impl_->kv_generation = 0;
    impl_->capabilities  = InferenceCapability::NONE;
}

// ── run_inference ─────────────────────────────────────────────────────────────

Result<InferenceResult> BitnetEngine::run_inference(const InferenceRequest &req) {
    if (!impl_->loaded) {
        return tl::unexpected(
            MONDAY_ERROR(MONDAY_ERR_MODEL_LOAD_FAILED, "engine not loaded"));
    }

    // Clamp parameters per spec
    (void)std::max(0.0f, std::min(2.0f, req.temperature));
    (void)std::max(0.0f, std::min(1.0f, req.top_p));
    uint32_t max_tokens = (req.max_new_tokens == 0) ? 512u : req.max_new_tokens;

    // Build stub response
    std::string prompt   = req.prompt ? req.prompt : "";
    std::string snippet  = prompt.substr(0, std::min<std::size_t>(50, prompt.size()));
    if (prompt.size() > 50) snippet += "...";
    std::string stub_resp = "[BitNet stub] Echo: " + snippet;

    // Tokenise by whitespace (one word = one token for the stub)
    std::vector<std::string> tokens;
    {
        std::istringstream ss(stub_resp);
        std::string tok;
        while (ss >> tok) tokens.push_back(tok + " ");
        if (!tokens.empty()) tokens.back().pop_back();
    }

    InferenceResult result{};
    result.prompt_tokens = 0;

    if (req.on_token) {
        // ── Streaming mode ────────────────────────────────────────────────────
        result.text = nullptr;   // streaming: text is null
        uint32_t delivered = 0;
        for (const auto &tok : tokens) {
            if (delivered >= max_tokens) break;
            bool cont = req.on_token(static_cast<int32_t>(delivered), tok.c_str());
            delivered++;
            if (!cont) break;  // cancellation
        }
        result.token_count  = delivered;
        result.was_truncated = (delivered >= max_tokens);
    } else {
        // ── Blocking mode ─────────────────────────────────────────────────────
        size_t len = stub_resp.size();
        char *text = static_cast<char *>(monday_alloc(len + 1));
        if (!text) {
            return tl::unexpected(
                MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "monday_alloc for result.text failed"));
        }
        std::memcpy(text, stub_resp.c_str(), len + 1);
        result.text         = text;
        result.token_count  = static_cast<uint32_t>(tokens.size());
        result.was_truncated = (result.token_count >= max_tokens);
    }

    impl_->kv_generation++;
    return result;
}

// ── KV cache ──────────────────────────────────────────────────────────────────

KVSnapshot BitnetEngine::get_kv_cache() const {
    KVSnapshot snap{};
    snap.kv_data      = nullptr;
    snap.kv_size_bytes = 0;
    snap.token_count  = 0;
    snap.generation   = impl_->kv_generation;
    return snap;
}

void BitnetEngine::reset_context() {
    impl_->kv_generation++;
}

// ── Capabilities ──────────────────────────────────────────────────────────────

InferenceCapability BitnetEngine::get_capabilities() const {
    return impl_->capabilities;
}

} // namespace monday
