// tests/unit/test_tier_selector.cpp
//
// Unit tests for the Vector Index Tier Selector.

#include <catch2/catch_test_macros.hpp>
#include "monday/rag/tier_selector.h"
#include "monday/rag/ingestion_pipeline.h"
#include "monday/storage/db.h"
#include "monday/storage/migrations.h"
#include "monday/core/allocator.h"
#include <sqlite3.h>

using namespace monday;

TEST_CASE("TierSelector: transition threshold boundary and database config storage", "[rag][selector]") {
    const char *db_path = "/tmp/test_monday_tier.db";
    const char *diskann_path = "/tmp/test_monday_diskann_tier.bin";
    remove(db_path);
    remove(diskann_path);

    monday_allocator_init(1024*1024, 512*1024, 512*1024, 2*1024*1024);

    monday_db_t *db = nullptr;
    auto open_err = monday_db_open(db_path, nullptr, &db);
    REQUIRE(open_err.code == MONDAY_OK);

    auto mig_err = monday_migrations_run(db);
    REQUIRE(mig_err.code == MONDAY_OK);

    MiniLMEmbedder embedder;
    REQUIRE(embedder.init("dummy").has_value());

    // Setup selector with a threshold of 3 chunks
    TierSelector selector(diskann_path, db, 3);
    IngestionPipeline pipeline(db, &embedder, &selector);

    // 1. Initial count is 0 -> should start as MRL
    auto idx_res = selector.get_active_index();
    REQUIRE(idx_res.has_value());
    REQUIRE(selector.get_current_tier() == IndexTier::MRL);

    // 2. Ingest doc1 with content that stays below threshold
    // Each ingest of a short text (< 512 words) produces 1 chunk.
    REQUIRE(pipeline.ingest_document("doc1", "one").has_value());
    REQUIRE(pipeline.ingest_document("doc2", "two").has_value());
    REQUIRE(selector.get_current_tier() == IndexTier::MRL);

    // 3. Ingest 2 more single-chunk docs to cross the threshold of 3
    REQUIRE(pipeline.ingest_document("doc3", "three").has_value());
    REQUIRE(pipeline.ingest_document("doc4", "four").has_value());
    // Now count = 4 > threshold of 3 -> should have switched to DISKANN
    REQUIRE(selector.get_current_tier() == IndexTier::DISKANN);

    // 4. Verify DB configuration table holds the state 'diskann'
    sqlite3 *sqlite = monday_db_handle(db);
    sqlite3_stmt *stmt = NULL;
    int res = sqlite3_prepare_v2(sqlite, "SELECT value FROM config WHERE key = 'vector_index_tier';", -1, &stmt, NULL);
    REQUIRE(res == SQLITE_OK);
    
    REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);
    const char *val = (const char*)sqlite3_column_text(stmt, 0);
    REQUIRE(strcmp(val, "diskann") == 0);

    sqlite3_finalize(stmt);

    monday_db_close(db);
    remove(db_path);
    remove(diskann_path);
}
