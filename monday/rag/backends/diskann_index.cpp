// monday/rag/backends/diskann_index.cpp
//
// DiskANN disk-resident vector index implementation.

#include "monday/rag/backends/diskann_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace monday {

DiskANNIndex::DiskANNIndex(const std::string &index_path, size_t dim)
    : index_path_(index_path), dim_(dim) {}

DiskANNIndex::~DiskANNIndex() {}

Result<void> DiskANNIndex::build(const EmbeddingBatch &batch) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string tmp_path = index_path_ + ".tmp";
    FILE *f = fopen(tmp_path.c_str(), "wb");
    if (!f) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to open temporary index file for write"));
    }

    FileHeader header;
    memcpy(header.magic, "MOND_ANN", 8);
    header.version = 1;
    header.dim = (uint32_t)dim_;
    header.item_count = (uint32_t)batch.size();

    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        remove(tmp_path.c_str());
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to write index header"));
    }

    for (const auto &item : batch) {
        uint64_t id = item.id;
        if (fwrite(&id, sizeof(id), 1, f) != 1 ||
            fwrite(item.vec, sizeof(float), dim_, f) != dim_) {
            fclose(f);
            remove(tmp_path.c_str());
            return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to write index items"));
        }
    }

    fclose(f);

    // Atomic rename (Copy-on-Write)
    if (rename(tmp_path.c_str(), index_path_.c_str()) != 0) {
        remove(tmp_path.c_str());
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "failed to atomically replace index file"));
    }

    return {};
}

Result<Results> DiskANNIndex::query(const float *vec, uint32_t k) {
    std::lock_guard<std::mutex> lock(mutex_);

    FILE *f = fopen(index_path_.c_str(), "rb");
    if (!f) {
        // Index does not exist yet; return empty results
        return Results();
    }

    FileHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "failed to read index header"));
    }

    if (memcmp(header.magic, "MOND_ANN", 8) != 0 || header.dim != dim_) {
        fclose(f);
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "invalid index file format or dimensions"));
    }

    std::vector<SearchResult> scored;
    scored.reserve(header.item_count);

    std::vector<float> item_vec(dim_);
    for (uint32_t i = 0; i < header.item_count; i++) {
        uint64_t id;
        if (fread(&id, sizeof(id), 1, f) != 1 ||
            fread(item_vec.data(), sizeof(float), dim_, f) != dim_) {
            fclose(f);
            return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "failed to read index item"));
        }

        // Cosine similarity
        float dot = 0.0f;
        for (size_t d = 0; d < dim_; d++) {
            dot += vec[d] * item_vec[d];
        }
        float score = (dot + 1.0f) / 2.0f;

        SearchResult res;
        res.id = id;
        res.score = score;
        scored.push_back(res);
    }

    fclose(f);

    std::sort(scored.begin(), scored.end(), [](const SearchResult &a, const SearchResult &b) {
        return a.score > b.score;
    });

    uint32_t limit = std::min(k, (uint32_t)scored.size());
    return Results(scored.begin(), scored.begin() + limit);
}

Result<void> DiskANNIndex::insert(uint64_t id, const float *vec) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string tmp_path = index_path_ + ".tmp";
    FILE *src = fopen(index_path_.c_str(), "rb");
    FILE *dst = fopen(tmp_path.c_str(), "wb");

    if (!dst) {
        if (src) fclose(src);
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to open temp file for insert"));
    }

    uint32_t count = 0;
    if (src) {
        FileHeader header;
        if (fread(&header, sizeof(header), 1, src) == 1) {
            count = header.item_count;
            // Write updated header
            header.item_count++;
            fwrite(&header, sizeof(header), 1, dst);

            // Copy all old entries
            std::vector<uint8_t> buffer(sizeof(uint64_t) + dim_ * sizeof(float));
            for (uint32_t i = 0; i < count; i++) {
                fread(buffer.data(), 1, buffer.size(), src);
                fwrite(buffer.data(), 1, buffer.size(), dst);
            }
        }
        fclose(src);
    } else {
        // Create new file
        FileHeader header;
        memcpy(header.magic, "MOND_ANN", 8);
        header.version = 1;
        header.dim = (uint32_t)dim_;
        header.item_count = 1;
        fwrite(&header, sizeof(header), 1, dst);
    }

    // Write new entry
    fwrite(&id, sizeof(id), 1, dst);
    fwrite(vec, sizeof(float), dim_, dst);

    fclose(dst);

    // Atomic rename
    if (rename(tmp_path.c_str(), index_path_.c_str()) != 0) {
        remove(tmp_path.c_str());
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "failed to atomically insert item"));
    }

    return {};
}

Result<void> DiskANNIndex::verify_integrity() {
    std::lock_guard<std::mutex> lock(mutex_);

    FILE *f = fopen(index_path_.c_str(), "rb");
    if (!f) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "index file not found"));
    }

    FileHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "failed to read index header"));
    }

    if (memcmp(header.magic, "MOND_ANN", 8) != 0 || header.dim != dim_) {
        fclose(f);
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "integrity check: invalid file magic or dimensions"));
    }

    // Verify file size matches entry count
    fseek(f, 0, SEEK_END);
    long actual_size = ftell(f);
    fclose(f);

    long expected_size = sizeof(FileHeader) + header.item_count * (sizeof(uint64_t) + dim_ * sizeof(float));
    if (actual_size != expected_size) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "integrity check: file size mismatch"));
    }

    return {};
}

} // namespace monday
