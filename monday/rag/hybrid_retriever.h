// monday/rag/hybrid_retriever.h
//
// Hybrid retriever (Architecture §2.6).
// Combines SQLite FTS5 lexical search and vector embedding re-ranking.

#pragma once

#include "monday/core/result.h"
#include "monday/storage/db.h"
#include "monday/rag/embedding/minilm_embedder.h"
#include "monday/rag/tier_selector.h"
#include <string>
#include <vector>

namespace monday {

struct HybridResult {
    int64_t     chunk_id;
    std::string doc_id;
    std::string text_content;
    float       score;
};

class HybridRetriever {
public:
    HybridRetriever(monday_db_t *db, MiniLMEmbedder *embedder, TierSelector *selector);
    ~HybridRetriever();

    // Retrieves top-k relevant chunks matching the query string.
    Result<std::vector<HybridResult>> retrieve(const std::string &query_text, uint32_t k = 5);

private:
    monday_db_t    *db_;
    MiniLMEmbedder *embedder_;
    TierSelector   *selector_;
};

} // namespace monday
