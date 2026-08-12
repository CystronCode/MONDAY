// monday/rag/ingestion_pipeline.h
//
// Ingestion pipeline for RAG (Architecture §2.6).
// Handles document chunking (512 tokens max, 64 overlap), embedding, and indexing.

#pragma once

#include "monday/core/result.h"
#include "monday/storage/db.h"
#include "monday/rag/embedding/minilm_embedder.h"
#include "monday/rag/tier_selector.h"
#include <string>
#include <vector>

namespace monday {

class IngestionPipeline {
public:
    IngestionPipeline(monday_db_t *db, MiniLMEmbedder *embedder, TierSelector *selector);
    ~IngestionPipeline();

    // Ingests document content: chunks it, computes embeddings, saves to DB,
    // and triggers tier selector updates.
    Result<void> ingest_document(const std::string &doc_id, const std::string &text);

    // Utility to split raw text into overlapping word chunks.
    static std::vector<std::string> chunk_text(const std::string &text, size_t chunk_size = 512, size_t overlap = 64);

private:
    monday_db_t    *db_;
    MiniLMEmbedder *embedder_;
    TierSelector   *selector_;
};

} // namespace monday
