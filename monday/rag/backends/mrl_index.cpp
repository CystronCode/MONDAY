// monday/rag/backends/mrl_index.cpp
//
// MRL in-memory vector index.

#include "monday/rag/backends/mrl_index.h"
#include <algorithm>
#include <cmath>

namespace monday {

MRLIndex::MRLIndex(size_t dim) : dim_(dim) {}

MRLIndex::~MRLIndex() {}

Result<void> MRLIndex::build(const EmbeddingBatch &batch) {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.clear();
    items_.reserve(batch.size());

    for (const auto &item : batch) {
        IndexItem index_item;
        index_item.id = item.id;
        index_item.vec.assign(item.vec, item.vec + dim_);
        items_.push_back(index_item);
    }

    return {};
}

Result<Results> MRLIndex::query(const float *vec, uint32_t k) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (items_.empty() || k == 0) return Results();

    std::vector<SearchResult> scored;
    scored.reserve(items_.size());

    for (const auto &item : items_) {
        float dot = 0.0f;
        for (size_t i = 0; i < dim_; i++) {
            dot += vec[i] * item.vec[i];
        }
        float score = (dot + 1.0f) / 2.0f; // Cosine similarity normalized to [0,1]
        
        SearchResult res;
        res.id = item.id;
        res.score = score;
        scored.push_back(res);
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(), [](const SearchResult &a, const SearchResult &b) {
        return a.score > b.score;
    });

    uint32_t limit = std::min(k, (uint32_t)scored.size());
    return Results(scored.begin(), scored.begin() + limit);
}

Result<void> MRLIndex::insert(uint64_t id, const float *vec) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    IndexItem index_item;
    index_item.id = id;
    index_item.vec.assign(vec, vec + dim_);
    items_.push_back(index_item);

    return {};
}

} // namespace monday
