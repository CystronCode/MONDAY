// monday/rag/backends/mrl_index.h
//
// Matryoshka Representation Learning (MRL) in-memory vector index (Architecture §2.6).

#pragma once

#include "monday/rag/IVectorIndex.h"
#include <vector>
#include <mutex>

namespace monday {

class MRLIndex : public IVectorIndex {
public:
    MRLIndex(size_t dim = 384);
    ~MRLIndex() override;

    Result<void> build(const EmbeddingBatch &batch) override;
    Result<Results> query(const float *vec, uint32_t k) override;
    Result<void> insert(uint64_t id, const float *vec) override;

private:
    size_t dim_;
    struct IndexItem {
        uint64_t           id;
        std::vector<float> vec;
    };
    std::vector<IndexItem> items_;
    std::mutex             mutex_;
};

} // namespace monday
