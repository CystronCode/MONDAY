// tests/integration/test_rag_pipeline.cpp
//
// Integration test for the Phase 3 RAG Pipeline (Ingestion to Retrieval).

#include <catch2/catch_test_macros.hpp>
#include "monday/rag/ingestion_pipeline.h"
#include "monday/rag/hybrid_retriever.h"
#include "monday/storage/db.h"
#include "monday/storage/migrations.h"
#include "monday/core/allocator.h"
#include <string.h>

using namespace monday;

TEST_CASE("RAG Pipeline: End-to-end document ingestion and hybrid retrieval", "[rag][integration]") {
    const char *db_path = "/tmp/test_monday_rag_pipeline.db";
    const char *diskann_path = "/tmp/test_monday_rag_pipeline_diskann.bin";
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

    TierSelector selector(diskann_path, db, 100);
    IngestionPipeline pipeline(db, &embedder, &selector);

    // 1. Ingest a document
    std::string doc_text = "This is a document about offline AI assistants. "
                           "The assistant runs entirely on device without cloud APIs. "
                           "It targets old computers with spinning disk hard drives. "
                           "The memory footprint is kept under one gigabyte of RAM. "
                           "Security is enforced at the system call boundary layer.";
    
    auto ingest_res = pipeline.ingest_document("doc1", doc_text);
    REQUIRE(ingest_res.has_value());

    // 2. Perform hybrid retrieval
    HybridRetriever retriever(db, &embedder, &selector);
    auto retrieve_res = retriever.retrieve("offline AI assistant security layer", 1);
    REQUIRE(retrieve_res.has_value());

    const auto &results = retrieve_res.value();
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].doc_id == "doc1");
    REQUIRE(results[0].score > 0.5f);

    monday_db_close(db);
    remove(db_path);
    remove(diskann_path);
}
