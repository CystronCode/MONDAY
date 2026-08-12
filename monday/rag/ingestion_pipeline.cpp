// monday/rag/ingestion_pipeline.cpp
//
// Ingestion pipeline implementation.

#include "monday/rag/ingestion_pipeline.h"
#include "monday/engram/semantic_store.h"
#include <sstream>

namespace monday {

IngestionPipeline::IngestionPipeline(monday_db_t *db, MiniLMEmbedder *embedder, TierSelector *selector)
    : db_(db), embedder_(embedder), selector_(selector) {}

IngestionPipeline::~IngestionPipeline() {}

Result<void> IngestionPipeline::ingest_document(const std::string &doc_id, const std::string &text) {
    if (text.empty()) return {};

    // 1. Chunk the raw text
    std::vector<std::string> chunks = chunk_text(text, 512, 64);

    // 2. Embed and insert each chunk
    for (const auto &chunk : chunks) {
        auto embed_res = embedder_->embed(chunk);
        if (!embed_res) {
            return tl::unexpected(embed_res.error());
        }

        const auto &vec = embed_res.value();
        monday_error_t insert_err = monday_semantic_store_insert(
            db_, doc_id.c_str(), chunk.c_str(), vec.data(), vec.size());
        
        if (insert_err.code != MONDAY_OK) {
            return tl::unexpected(insert_err);
        }
    }

    // 3. Trigger tier selector to evaluate if counts crossed boundary limits
    auto eval_res = selector_->evaluate_and_update();
    if (!eval_res) {
        return tl::unexpected(eval_res.error());
    }

    return {};
}

std::vector<std::string> IngestionPipeline::chunk_text(const std::string &text, size_t chunk_size, size_t overlap) {
    std::vector<std::string> words;
    std::stringstream ss(text);
    std::string word;
    while (ss >> word) {
        words.push_back(word);
    }

    std::vector<std::string> chunks;
    if (words.empty()) return chunks;

    size_t stride = (chunk_size > overlap) ? (chunk_size - overlap) : 1;

    for (size_t i = 0; i < words.size(); i += stride) {
        size_t end = std::min(i + chunk_size, words.size());
        std::string chunk;
        for (size_t w = i; w < end; w++) {
            if (w > i) chunk += " ";
            chunk += words[w];
        }
        chunks.push_back(chunk);
        if (end == words.size()) break;
    }

    return chunks;
}

} // namespace monday
