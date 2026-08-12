// monday/inference/backends/llama_engine.h
//
// IInferenceEngine adapter wrapping llama.cpp (GGUF fallback).

#pragma once
#include "monday/inference/IInferenceEngine.h"

namespace monday {

class LlamaEngine final : public IInferenceEngine {
public:
    LlamaEngine();
    ~LlamaEngine() override;

    Result<void>            load_model(const char* model_path,
                                       const char* manifest_path) override;
    void                    unload_model() override;
    Result<InferenceResult> run_inference(const InferenceRequest& req) override;
    KVSnapshot              get_kv_cache() const override;
    void                    reset_context() override;
    InferenceCapability     get_capabilities() const override;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace monday
