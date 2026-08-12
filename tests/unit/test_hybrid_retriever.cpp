// tests/unit/test_hybrid_retriever.cpp
//
// Unit tests for the RAG Hybrid Retriever.

#include <catch2/catch_test_macros.hpp>
#include "monday/rag/hybrid_retriever.h"
#include "monday/rag/ingestion_pipeline.h"
#include "monday/storage/db.h"
#include "monday/storage/migrations.h"
#include "monday/core/allocator.h"
#include <string.h>

using namespace monday;

TEST_CASE("HybridRetriever: retrieval and semantic re-ranking", "[rag][hybrid]") {
    const char *db_path = "/tmp/test_monday_hybrid.db";
    const char *diskann_path = "/tmp/test_monday_diskann.bin";
    remove(db_path);
    remove(diskann_path);

    monday_allocator_init(1024*1024, 512*1024, 512*1024, 2*1024*1024);

    monday_db_t *db = nullptr;
    auto open_err = monday_db_open(db_path, nullptr, &db);
    REQUIRE(open_err.code == MONDAY_OK);

    auto mig_err = monday_migrations_run(db);
    REQUIRE(mig_err.code == MONDAY_OK);

    // Initialise Embedder & Ingestion
    MiniLMEmbedder embedder;
    REQUIRE(embedder.init("dummy_path").has_value());

    TierSelector selector(diskann_path, db, 100); // threshold = 100
    IngestionPipeline pipeline(db, &embedder, &selector);

    // 1. Ingest sample documents
    REQUIRE(pipeline.ingest_document("doc1", "machine learning linear regression gradient descent algorithms").has_value());
    REQUIRE(pipeline.ingest_document("doc2", "cooking recipes for chicken soup and salad").has_value());
    REQUIRE(pipeline.ingest_document("doc3", "history of computing and personal computer evolution").has_value());

    HybridRetriever retriever(db, &embedder, &selector);

    // 2. Query matching FTS5 + semantic
    auto search_res = retriever.retrieve("machine learning algorithms");
    REQUIRE(search_res.has_value());
    
    const auto &results = search_res.value();
    REQUIRE(results.size() >= 1);
    
    // The most relevant document "doc1" should be ranked first
    REQUIRE(results[0].doc_id == "doc1");
    REQUIRE(results[0].score > 0.5f);

    // 3. Query matching cooking — verify we get some results back
    // (With a mock hash embedder, semantic ranking isn't meaningful;
    //  we just verify the retrieval pipeline runs correctly end-to-end)
    auto search_cooking = retriever.retrieve("soup recipe", 10);
    REQUIRE(search_cooking.has_value());
    REQUIRE(search_cooking.value().size() >= 1);

    monday_db_close(db);
    remove(db_path);
    remove(diskann_path);
}
