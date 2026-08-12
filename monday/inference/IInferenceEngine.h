// monday/inference/IInferenceEngine.h
//
// Exact interface specification from Monday Architecture v3 §2.2.
// This is the sole ABI contract between inference backends and all calling code.
//
// Concurrency contract:
//   load_model() / unload_model() : init thread only, never concurrent.
//   run_inference() / get_kv_cache() / reset_context() : inference thread only;
//     serialised by the caller's inference mutex — not the engine's responsibility.
//   get_capabilities() : any thread, any time after load_model() returns.

#pragma once
#include "monday/core/result.h"   // monday::Result<T>

#include <cstdint>
#include <functional>

namespace monday {

// Capability flags returned by get_capabilities().
// Constant after load_model() completes. Safe to read from any thread.
enum class InferenceCapability : uint32_t {
    NONE              = 0,
    STREAMING         = 1 << 0,  // supports token-by-token callback
    KV_CACHE          = 1 << 1,  // maintains a KV cache across calls
    AVX2              = 1 << 2,  // built with AVX2 kernels
    SSE4_2            = 1 << 3,  // built with SSE4.2 kernels
    INT4_QUANTIZED    = 1 << 4,  // uses INT4 KV cache quantisation
    CONTEXT_SLIDING   = 1 << 5,  // supports sliding window eviction
    GRACEFUL_DEGRADED = 1 << 6,  // running fallback model (Qwen2.5-0.5B)
};
inline InferenceCapability operator|(InferenceCapability a, InferenceCapability b) {
    return static_cast<InferenceCapability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool operator&(InferenceCapability a, InferenceCapability b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// Token callback: called on the inference thread for every generated token.
// MUST be non-blocking. MUST NOT call back into the engine.
// Return true to continue generation; return false to cancel.
using TokenCallback = std::function<bool(int32_t token_id, const char* token_text)>;

struct InferenceRequest {
    const char*   prompt;         // UTF-8 null-terminated; owned by caller; valid for duration
    uint32_t      max_new_tokens; // hard ceiling; 0 = engine default (512)
    float         temperature;    // 0.0 = greedy; clamped [0.0, 2.0]
    float         top_p;          // nucleus sampling; 0.0 = disabled; clamped [0.0, 1.0]
    TokenCallback on_token;       // nullable; if null, run_inference() blocks and returns full text
    bool          append_to_kv;   // true = append to existing KV context; false = reset first
};

struct InferenceResult {
    char*    text;          // monday_alloc()'d; caller MUST call monday_free()
    uint32_t token_count;   // tokens generated
    uint32_t prompt_tokens; // tokens consumed from prompt
    bool     was_truncated; // true if stopped at max_new_tokens
};

// KV cache snapshot — non-owning view into engine's internal arena.
// Valid ONLY until the next call to run_inference() or reset_context().
// Callers must NOT free kv_data. generation counter detects staleness.
struct KVSnapshot {
    const void* kv_data;
    size_t      kv_size_bytes;
    uint32_t    token_count;
    uint32_t    generation;  // increments on every run_inference() or reset_context()
};

class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;

    // Load model. Verifies SHA-256 against signed manifest before mmap'ing weights.
    // Thread-safety: init thread only. Never call concurrently or from inference thread.
    virtual Result<void> load_model(const char* model_path,
                                    const char* manifest_path) = 0;

    // Unload model. Call only after inference thread has exited.
    virtual void unload_model() = 0;

    // Run inference. ONLY called from the inference thread.
    // NOT re-entrant. NOT thread-safe.
    // Caller serialises with the inference mutex — not the engine's responsibility.
    //
    // Streaming: if req.on_token != null, tokens delivered via callback as generated.
    //   Returns after all tokens delivered or on_token returns false (cancellation).
    //   result.text is null in streaming mode; caller assembles the string.
    // Blocking: if req.on_token == null, blocks and populates result.text.
    //
    // Cancellation: the ONLY path is returning false from on_token.
    // Error mid-stream: on_token called with token_id = -1, token_text = null,
    //   then run_inference() returns an error Result.
    virtual Result<InferenceResult> run_inference(
        const InferenceRequest& req) = 0;

    // Returns non-owning KV cache snapshot. Inference thread only.
    virtual KVSnapshot get_kv_cache() const = 0;

    // Reset KV cache. Inference thread only (or while holding inference mutex).
    virtual void reset_context() = 0;

    // Capability bitmap. Constant after load_model(). Any thread, any time.
    virtual InferenceCapability get_capabilities() const = 0;
};

} // namespace monday
