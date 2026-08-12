// monday/rag/embedding/minilm_embedder.h
//
// MiniLM embedding model interface (384-dimensional vectors).

#pragma once

#include "monday/core/result.h"
#include <vector>
#include <string>

namespace monday {

class MiniLMEmbedder {
public:
    MiniLMEmbedder();
    ~MiniLMEmbedder();

    // Load the embedder using the model path.
    Result<void> init(const std::string &model_path);

    // Compute a 384-dimensional embedding vector for the text.
    Result<std::vector<float>> embed(const std::string &text);

    // Calculate cosine similarity normalized to [0,1] range: (dot + 1) / 2
    static float cosine_similarity(const std::vector<float> &a, const std::vector<float> &b);

    size_t get_dimension() const { return 384; }

private:
    std::string model_path_;
    bool        inited_;
};

} // namespace monday
