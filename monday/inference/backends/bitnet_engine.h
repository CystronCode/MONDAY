// monday/inference/backends/bitnet_engine.h
//
// IInferenceEngine adapter wrapping bitnet.cpp (Microsoft Research prototype).

#pragma once
#include "monday/inference/IInferenceEngine.h"

namespace monday {

class BitnetEngine final : public IInferenceEngine {
public:
    BitnetEngine();
    ~BitnetEngine() override;

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
