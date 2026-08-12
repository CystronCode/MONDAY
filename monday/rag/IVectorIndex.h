// monday/rag/IVectorIndex.h
//
// Abstract vector index interface (Architecture §2.6).

#pragma once

#include "monday/core/result.h"
#include <stdint.h>
#include <vector>

namespace monday {

struct EmbeddingItem {
    uint64_t     id;
    const float *vec;
};

using EmbeddingBatch = std::vector<EmbeddingItem>;

struct SearchResult {
    uint64_t id;
    float    score;
};

using Results = std::vector<SearchResult>;

class IVectorIndex {
public:
    virtual ~IVectorIndex() = default;

    // Build the index from a batch of embeddings.
    virtual Result<void> build(const EmbeddingBatch &batch) = 0;

    // Query the index for the top-k nearest neighbors.
    virtual Result<Results> query(const float *vec, uint32_t k) = 0;

    // Dynamically insert a single embedding vector into the index.
    virtual Result<void> insert(uint64_t id, const float *vec) = 0;
};

} // namespace monday
