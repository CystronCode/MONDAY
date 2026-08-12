// monday/inference/backends/llama_engine.cpp
//
// IInferenceEngine implementation wrapping llama.cpp for GGUF fallback models (Qwen2.5-0.5B).

#include "monday/inference/backends/llama_engine.h"
#include "monday/inference/cpuid_dispatch.h"
#include "monday/security/model_verify.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include "llama.h"

#include <string>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <sstream>

namespace monday {

// ── Impl ──────────────────────────────────────────────────────────────────────

struct LlamaEngine::Impl {
    std::string         model_path;
    std::string         manifest_path;
    bool                loaded        = false;
    uint32_t            kv_generation = 0;
    InferenceCapability capabilities  = InferenceCapability::NONE;

    llama_model*        model         = nullptr;
    llama_context*      ctx           = nullptr;
    const llama_vocab*  vocab         = nullptr;
};

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────

LlamaEngine::LlamaEngine()  : impl_(new Impl{}) {}
LlamaEngine::~LlamaEngine() {
    unload_model();
    delete impl_;
}

// ── load_model ────────────────────────────────────────────────────────────────

Result<void> LlamaEngine::load_model(const char *model_path,
                                      const char *manifest_path) {
    if (!model_path || !manifest_path) {
        return tl::unexpected(
            MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null path argument to load_model"));
    }

    if (impl_->loaded) {
        unload_model();
    }

    monday_model_manifest_t manifest;
    monday_error_t err = monday_model_verify(manifest_path, &manifest);
    if (err.code != MONDAY_OK) {
        return tl::unexpected(err);
    }

    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;  // Pure CPU execution per Monday specs

    impl_->model = llama_model_load_from_file(model_path, mparams);
    if (!impl_->model) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "llama_engine: llama_model_load_from_file failed");
        return tl::unexpected(
            MONDAY_ERROR(MONDAY_ERR_MODEL_LOAD_FAILED, "failed to load llama model file"));
    }

    impl_->vocab = llama_model_get_vocab(impl_->model);

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx     = 512;  // 512-token sliding window per architecture §2.3
    cparams.n_threads = 4;

    impl_->ctx = llama_init_from_model(impl_->model, cparams);
    if (!impl_->ctx) {
        llama_model_free(impl_->model);
        impl_->model = nullptr;
        return tl::unexpected(
            MONDAY_ERROR(MONDAY_ERR_MODEL_LOAD_FAILED, "failed to create llama context"));
    }

    impl_->model_path    = model_path;
    impl_->manifest_path = manifest_path;
    impl_->loaded        = true;
    impl_->kv_generation = 0;

    impl_->capabilities = InferenceCapability::STREAMING
                        | InferenceCapability::KV_CACHE
                        | InferenceCapability::GRACEFUL_DEGRADED;
    if (g_cpu_features == MONDAY_CPU_AVX2) {
        impl_->capabilities = impl_->capabilities | InferenceCapability::AVX2;
    } else if (g_cpu_features == MONDAY_CPU_SSE4_2) {
        impl_->capabilities = impl_->capabilities | InferenceCapability::SSE4_2;
    }

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "llama_engine: GGUF model loaded successfully");
    return {};
}

// ── unload_model ──────────────────────────────────────────────────────────────

void LlamaEngine::unload_model() {
    if (impl_->ctx) {
        llama_free(impl_->ctx);
        impl_->ctx = nullptr;
    }
    if (impl_->model) {
        llama_model_free(impl_->model);
        impl_->model = nullptr;
    }
    impl_->vocab         = nullptr;
    impl_->loaded        = false;
    impl_->kv_generation = 0;
    impl_->capabilities  = InferenceCapability::NONE;
}

// ── run_inference ─────────────────────────────────────────────────────────────

Result<InferenceResult> LlamaEngine::run_inference(const InferenceRequest &req) {
    if (!impl_->loaded || !impl_->ctx || !impl_->model) {
        return tl::unexpected(
            MONDAY_ERROR(MONDAY_ERR_MODEL_LOAD_FAILED, "llama engine not loaded"));
    }

    float temp = std::max(0.0f, std::min(2.0f, req.temperature));
    float top_p = std::max(0.0f, std::min(1.0f, req.top_p));
    uint32_t max_tokens = (req.max_new_tokens == 0) ? 512u : req.max_new_tokens;

    std::string prompt_str = req.prompt ? req.prompt : "";

    // Format prompt using ChatML template if not already present
    std::string formatted_prompt;
    if (prompt_str.find("<|im_start|>") == std::string::npos) {
        formatted_prompt = "<|im_start|>system\nYou are Monday, a concise offline AI operating system assistant.<|im_end|>\n<|im_start|>user\n" + prompt_str + "<|im_end|>\n<|im_start|>assistant\n";
    } else {
        formatted_prompt = prompt_str;
    }

    if (!req.append_to_kv) {
        reset_context();
    }

    // Tokenize prompt
    std::vector<llama_token> prompt_tokens(formatted_prompt.size() + 32);
    int n_prompt = llama_tokenize(impl_->vocab,
                                  formatted_prompt.c_str(),
                                  static_cast<int32_t>(formatted_prompt.size()),
                                  prompt_tokens.data(),
                                  static_cast<int32_t>(prompt_tokens.size()),
                                  true,   // add_special (BOS)
                                  true);  // parse_special
    if (n_prompt < 0) {
        prompt_tokens.resize(-n_prompt);
        n_prompt = llama_tokenize(impl_->vocab,
                                  formatted_prompt.c_str(),
                                  static_cast<int32_t>(formatted_prompt.size()),
                                  prompt_tokens.data(),
                                  static_cast<int32_t>(prompt_tokens.size()),
                                  true,
                                  true);
    }
    if (n_prompt <= 0) {
        return tl::unexpected(
            MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "failed to tokenize prompt"));
    }
    prompt_tokens.resize(n_prompt);

    // Evaluate prompt in batch
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), static_cast<int32_t>(prompt_tokens.size()));
    if (llama_decode(impl_->ctx, batch) != 0) {
        return tl::unexpected(
            MONDAY_ERROR(MONDAY_ERR_INFERENCE_FAILED, "llama_decode failed on prompt"));
    }

    // Setup samplers
    int32_t n_vocab = llama_vocab_n_tokens(impl_->vocab);
    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    llama_sampler *smpl = llama_sampler_chain_init(sparams);

    // Add repetition penalty (last 64 tokens, 1.15f multiplier) to prevent looping
    llama_sampler_chain_add(smpl, llama_sampler_init_penalties(n_vocab, 64, 1.15f, 0.0f, 0.0f));

    if (temp > 0.0f) {
        llama_sampler_chain_add(smpl, llama_sampler_init_temp(temp));
        if (top_p > 0.0f) {
            llama_sampler_chain_add(smpl, llama_sampler_init_top_p(top_p, 1));
        }
        llama_sampler_chain_add(smpl, llama_sampler_init_dist(1234));
    } else {
        llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
    }

    std::string generated_text;
    uint32_t generated_count = 0;
    bool truncated = false;

    // Generation loop
    while (generated_count < max_tokens) {
        llama_token new_tok = llama_sampler_sample(smpl, impl_->ctx, -1);
        llama_sampler_accept(smpl, new_tok);

        if (llama_vocab_is_eog(impl_->vocab, new_tok)) {
            break;
        }

        char piece_buf[256];
        int n_piece = llama_token_to_piece(impl_->vocab, new_tok, piece_buf, sizeof(piece_buf), 0, true);
        std::string piece_str = (n_piece > 0) ? std::string(piece_buf, n_piece) : "";

        if (piece_str.find("<|im_end|>") != std::string::npos || piece_str.find("<|endoftext|>") != std::string::npos) {
            break;
        }

        if (req.on_token) {
            bool cont = req.on_token(static_cast<int32_t>(generated_count), piece_str.c_str());
            generated_count++;
            if (!cont) {
                break;
            }
        } else {
            generated_text += piece_str;
            generated_count++;
        }

        llama_batch next_batch = llama_batch_get_one(&new_tok, 1);
        if (llama_decode(impl_->ctx, next_batch) != 0) {
            break;
        }
    }

    if (generated_count >= max_tokens) {
        truncated = true;
    }

    llama_sampler_free(smpl);
    impl_->kv_generation++;

    InferenceResult result{};
    result.prompt_tokens = static_cast<uint32_t>(n_prompt);
    result.token_count  = generated_count;
    result.was_truncated = truncated;

    if (req.on_token) {
        result.text = nullptr;
    } else {
        size_t len = generated_text.size();
        char *text = static_cast<char *>(monday_alloc(len + 1));
        if (!text) {
            return tl::unexpected(
                MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "monday_alloc for result.text failed"));
        }
        std::memcpy(text, generated_text.c_str(), len + 1);
        result.text = text;
    }

    return result;
}

// ── KV cache ──────────────────────────────────────────────────────────────────

KVSnapshot LlamaEngine::get_kv_cache() const {
    KVSnapshot snap{};
    snap.kv_data       = nullptr;
    snap.kv_size_bytes = 0;
    snap.token_count   = 0;
    snap.generation    = impl_->kv_generation;
    return snap;
}

void LlamaEngine::reset_context() {
    if (impl_->ctx) {
        llama_memory_t mem = llama_get_memory(impl_->ctx);
        if (mem) {
            llama_memory_clear(mem, true);
        }
    }
    impl_->kv_generation++;
}

// ── Capabilities ──────────────────────────────────────────────────────────────

InferenceCapability LlamaEngine::get_capabilities() const {
    return impl_->capabilities;
}

} // namespace monday
