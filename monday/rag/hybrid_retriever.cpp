// monday/rag/hybrid_retriever.cpp
//
// Hybrid retriever implementation.

#include "monday/rag/hybrid_retriever.h"
#include "monday/engram/semantic_store.h"
#include <algorithm>
#include <map>
#include <sqlite3.h>

namespace monday {

HybridRetriever::HybridRetriever(monday_db_t *db, MiniLMEmbedder *embedder, TierSelector *selector)
    : db_(db), embedder_(embedder), selector_(selector) {}

HybridRetriever::~HybridRetriever() {}

Result<std::vector<HybridResult>> HybridRetriever::retrieve(const std::string &query_text, uint32_t k) {
    if (query_text.empty() || k == 0) return std::vector<HybridResult>();

    // 1. Get query embedding
    auto embed_res = embedder_->embed(query_text);
    if (!embed_res) {
        return tl::unexpected(embed_res.error());
    }
    const auto &query_vec = embed_res.value();

    // 2. Fetch candidates using lexical SQLite FTS5 index
    monday_semantic_record_t *fts_records = nullptr;
    size_t fts_count = 0;
    
    monday_error_t query_err = monday_semantic_store_query_fts(db_, query_text.c_str(), &fts_records, &fts_count);
    if (query_err.code != MONDAY_OK) {
        return tl::unexpected(query_err);
    }

    std::vector<HybridResult> candidates;

    if (fts_count > 0) {
        // Calculate similarity for all lexical matches
        for (size_t i = 0; i < fts_count; i++) {
            const auto &rec = fts_records[i];
            
            // Re-rank: calculate similarity between query and candidate embedding
            std::vector<float> item_vec(rec.embedding, rec.embedding + rec.embedding_dim);
            float sim = MiniLMEmbedder::cosine_similarity(query_vec, item_vec);

            HybridResult hr;
            hr.chunk_id = rec.chunk_id;
            hr.doc_id = rec.doc_id;
            hr.text_content = rec.text_content;
            hr.score = sim;
            candidates.push_back(hr);
        }
        monday_semantic_records_free(fts_records, fts_count);
    }

    // 3. If lexical index returned fewer than k candidates (or we want to augment with vector search)
    if (candidates.size() < k) {
        auto active_index_res = selector_->get_active_index();
        if (active_index_res) {
            IVectorIndex *index = active_index_res.value();
            auto vector_res = index->query(query_vec.data(), k * 2);
            
            if (vector_res) {
                // To avoid duplicates, build set of existing IDs
                std::map<int64_t, bool> existing;
                for (const auto &c : candidates) {
                    existing[c.chunk_id] = true;
                }

                sqlite3 *sqlite = monday_db_handle(db_);
                
                // Fetch semantic records from SQLite for vector match IDs
                for (const auto &item : vector_res.value()) {
                    if (existing[item.id]) continue;
                    
                    sqlite3_stmt *stmt = NULL;
                    const char *sql = "SELECT doc_id, text_content FROM semantic_memory WHERE chunk_id = ?;";
                    if (sqlite3_prepare_v2(sqlite, sql, -1, &stmt, NULL) == SQLITE_OK) {
                        sqlite3_bind_int64(stmt, 1, item.id);
                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                            const char *doc_id = (const char*)sqlite3_column_text(stmt, 0);
                            const char *text = (const char*)sqlite3_column_text(stmt, 1);
                            
                            HybridResult hr;
                            hr.chunk_id = (int64_t)item.id;
                            hr.doc_id = doc_id ? doc_id : "";
                            hr.text_content = text ? text : "";
                            hr.score = item.score;
                            
                            candidates.push_back(hr);
                            existing[item.id] = true;
                        }
                        sqlite3_finalize(stmt);
                    }
                    if (candidates.size() >= k * 2) break;
                }
            }
        }
    }

    // 4. Sort all candidates and limit to top-k
    std::sort(candidates.begin(), candidates.end(), [](const HybridResult &a, const HybridResult &b) {
        return a.score > b.score;
    });

    if (candidates.size() > k) {
        candidates.resize(k);
    }

    return candidates;
}

} // namespace monday
