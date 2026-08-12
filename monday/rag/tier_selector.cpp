// monday/rag/tier_selector.cpp
//
// Tier selector implementation.

#include "monday/rag/tier_selector.h"
#include "monday/rag/backends/mrl_index.h"
#include "monday/rag/backends/diskann_index.h"
#include "monday/core/log.h"
#include <sqlite3.h>

namespace monday {

TierSelector::TierSelector(const std::string &diskann_path, monday_db_t *db, size_t threshold)
    : diskann_path_(diskann_path), db_(db), threshold_(threshold), current_tier_(IndexTier::MRL) {}

TierSelector::~TierSelector() {}

Result<IVectorIndex*> TierSelector::get_active_index() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_index_) {
        // Initial setup
        sqlite3 *sqlite = monday_db_handle(db_);
        size_t count = 0;
        
        sqlite3_stmt *stmt = NULL;
        int res = sqlite3_prepare_v2(sqlite, "SELECT COUNT(*) FROM semantic_memory;", -1, &stmt, NULL);
        if (res == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = (size_t)sqlite3_column_int64(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        if (count > threshold_) {
            current_tier_ = IndexTier::DISKANN;
            active_index_ = std::make_unique<DiskANNIndex>(diskann_path_);
        } else {
            current_tier_ = IndexTier::MRL;
            active_index_ = std::make_unique<MRLIndex>();
        }

        // Rebuild index from SQLite storage
        auto rebuild_res = rebuild_index_from_db(active_index_.get());
        if (!rebuild_res) {
            return tl::unexpected(rebuild_res.error());
        }
    }
    return active_index_.get();
}

Result<void> TierSelector::evaluate_and_update() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3 *sqlite = monday_db_handle(db_);
    size_t count = 0;
    
    sqlite3_stmt *stmt = NULL;
    int res = sqlite3_prepare_v2(sqlite, "SELECT COUNT(*) FROM semantic_memory;", -1, &stmt, NULL);
    if (res == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = (size_t)sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    } else {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to query semantic_memory size"));
    }

    IndexTier target_tier = (count > threshold_) ? IndexTier::DISKANN : IndexTier::MRL;
    if (target_tier != current_tier_ || !active_index_) {
        monday_log_write_kv_long(MONDAY_LOG_DIAGNOSTIC,
                                 "tier_selector: switching index tier",
                                 "old_tier", (long)current_tier_);

        std::unique_ptr<IVectorIndex> new_index;
        if (target_tier == IndexTier::DISKANN) {
            new_index = std::make_unique<DiskANNIndex>(diskann_path_);
        } else {
            new_index = std::make_unique<MRLIndex>();
        }

        // Populate new index from DB
        auto rebuild_res = rebuild_index_from_db(new_index.get());
        if (!rebuild_res) {
            return tl::unexpected(rebuild_res.error());
        }

        active_index_ = std::move(new_index);
        current_tier_ = target_tier;

        // Save new tier in DB config
        const char *config_sql = (target_tier == IndexTier::DISKANN) ?
            "INSERT OR REPLACE INTO config (key, value) VALUES ('vector_index_tier', 'diskann');" :
            "INSERT OR REPLACE INTO config (key, value) VALUES ('vector_index_tier', 'mrl');";
        
        sqlite3_exec(sqlite, config_sql, NULL, NULL, NULL);
    }

    return {};
}

Result<void> TierSelector::rebuild_index_from_db(IVectorIndex *index) {
    sqlite3 *sqlite = monday_db_handle(db_);
    sqlite3_stmt *stmt = NULL;

    const char *sql = "SELECT chunk_id, embedding FROM semantic_memory;";
    int res = sqlite3_prepare_v2(sqlite, sql, -1, &stmt, NULL);
    if (res != SQLITE_OK) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to query embeddings for index rebuild"));
    }

    EmbeddingBatch batch;
    std::vector<std::vector<float>> vectors_holder;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint64_t chunk_id = (uint64_t)sqlite3_column_int64(stmt, 0);
        const void *blob = sqlite3_column_blob(stmt, 1);
        int bytes = sqlite3_column_bytes(stmt, 1);
        
        size_t dim = bytes / sizeof(float);
        const float *floats = (const float*)blob;

        std::vector<float> vec(floats, floats + dim);
        vectors_holder.push_back(vec);

        EmbeddingItem item;
        item.id = chunk_id;
        item.vec = nullptr; // we will assign it after vector allocation is stable
        batch.push_back(item);
    }
    sqlite3_finalize(stmt);

    // Assign stable pointers
    for (size_t i = 0; i < batch.size(); i++) {
        batch[i].vec = vectors_holder[i].data();
    }

    // Build
    auto build_res = index->build(batch);
    if (!build_res) {
        return tl::unexpected(build_res.error());
    }

    return {};
}

} // namespace monday
