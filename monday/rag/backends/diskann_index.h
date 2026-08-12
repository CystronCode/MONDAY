// monday/rag/backends/diskann_index.h
//
// DiskANN disk-resident vector index (Architecture §2.6).

#pragma once

#include "monday/rag/IVectorIndex.h"
#include <string>
#include <mutex>

namespace monday {

class DiskANNIndex : public IVectorIndex {
public:
    DiskANNIndex(const std::string &index_path, size_t dim = 384);
    ~DiskANNIndex() override;

    Result<void> build(const EmbeddingBatch &batch) override;
    Result<Results> query(const float *vec, uint32_t k) override;
    Result<void> insert(uint64_t id, const float *vec) override;

    // Verify index file integrity.
    Result<void> verify_integrity();

private:
    std::string index_path_;
    size_t      dim_;
    std::mutex  mutex_;

    struct FileHeader {
        char     magic[8];      // "MOND_ANN"
        uint32_t version;       // 1
        uint32_t dim;           // 384
        uint32_t item_count;
    };
};

} // namespace monday
