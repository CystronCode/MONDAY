// tools/interactive_sandbox.cpp
//
// An interactive CLI playground for Phase 3 features of Monday:
//   - Encrypted storage (SQLCipher + migrations)
//   - Audit Log (append-only HMAC chained log)
//   - ENGRAM episodic memory commits
//   - RAG ingestion pipeline (chunking + embedding)
//   - RAG hybrid retrieval (lexical FTS5 + vector similarity re-ranking)

#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/storage/db.h"
#include "monday/storage/migrations.h"
#include "monday/security/audit_log.h"
#include "monday/engram/engram.h"
#include "monday/rag/embedding/minilm_embedder.h"
#include "monday/rag/tier_selector.h"
#include "monday/rag/ingestion_pipeline.h"
#include "monday/rag/hybrid_retriever.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

using namespace monday;

void print_help() {
    std::cout << "\n=== Monday Phase 3 Sandbox Commands ===\n"
              << "  /ingest <doc_id> <content>  - Chunk, embed, and index a document\n"
              << "  /query <search_text>        - Search using Hybrid lexical + vector retriever\n"
              << "  /turn <user> <assistant>    - Save a conversation turn into episodic memory\n"
              << "  /status                     - View DB record counts (turns, chunks)\n"
              << "  /verify                     - Verify cryptographic audit log integrity\n"
              << "  /help                       - Show this menu\n"
              << "  /exit                       - Close sandbox\n\n";
}

int main() {
    // 1. Initialize Allocator
    monday_allocator_init(4*1024*1024, 4*1024*1024, 4*1024*1024, 8*1024*1024);
    monday_log_init("sandbox.log", 10*1024*1024, 3);

    // Paths
    const char *db_path = "sandbox_monday.db";
    const char *audit_path = "sandbox_audit.log";
    const char *vector_path = "sandbox_vectors.bin";

    std::cout << "Starting Monday Phase 3 Interactive Sandbox...\n";
    std::cout << "Database path: " << db_path << "\n";
    std::cout << "Audit log path: " << audit_path << "\n";

    // 2. Initialize Audit Log
    monday_audit_log_init(audit_path);

    // 3. Open Encrypted DB & Run Migrations
    monday_db_t *db = nullptr;
    auto open_err = monday_db_open(db_path, nullptr, &db);
    if (open_err.code != MONDAY_OK) {
        std::cerr << "Failed to open encrypted database: " << open_err.message << "\n";
        monday_allocator_shutdown();
        return 1;
    }
    auto mig_err = monday_migrations_run(db);
    if (mig_err.code != MONDAY_OK) {
        std::cerr << "Failed to run database migrations: " << mig_err.message << "\n";
        monday_db_close(db);
        monday_allocator_shutdown();
        return 1;
    }

    // 4. Set up Embedder & Ingestion & Retrieval Components
    MiniLMEmbedder embedder;
    if (!embedder.init("dummy").has_value()) {
        std::cerr << "Failed to initialize mock MiniLM embedder.\n";
        monday_db_close(db);
        monday_allocator_shutdown();
        return 1;
    }

    TierSelector selector(vector_path, db, 100);
    IngestionPipeline pipeline(db, &embedder, &selector);
    HybridRetriever retriever(db, &embedder, &selector);

    print_help();

    std::string line;
    int turn_counter = 1;

    while (true) {
        std::cout << "sandbox> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line.empty()) continue;

        if (line == "/exit" || line == "/quit") {
            break;
        } else if (line == "/help") {
            print_help();
        } else if (line.rfind("/ingest ", 0) == 0) {
            std::string args = line.substr(8);
            std::string::size_type space_pos = args.find(' ');
            if (space_pos == std::string::npos) {
                std::cout << "Usage: /ingest <doc_id> <content>\n";
                continue;
            }
            std::string doc_id = args.substr(0, space_pos);
            std::string content = args.substr(space_pos + 1);

            std::cout << "Ingesting document '" << doc_id << "'...\n";
            auto res = pipeline.ingest_document(doc_id, content);
            if (res.has_value()) {
                std::cout << "Successfully ingested and indexed doc '" << doc_id << "'!\n";
            } else {
                std::cout << "Ingest failed: code " << res.error().code << " (" << res.error().message << ")\n";
            }
        } else if (line.rfind("/query ", 0) == 0) {
            std::string query_text = line.substr(7);
            std::cout << "Searching for: \"" << query_text << "\"\n";
            auto res = retriever.retrieve(query_text, 5);
            if (res.has_value()) {
                const auto &hits = res.value();
                std::cout << "Found " << hits.size() << " match(es):\n";
                for (size_t i = 0; i < hits.size(); ++i) {
                    std::cout << "  [" << i + 1 << "] Score: " << std::fixed << std::setprecision(4) << hits[i].score
                              << " | Doc ID: " << hits[i].doc_id << "\n"
                              << "      Content: \"" << hits[i].text_content << "\"\n";
                }
            } else {
                std::cout << "Query failed: code " << res.error().code << "\n";
            }
        } else if (line.rfind("/turn ", 0) == 0) {
            std::string args = line.substr(6);
            std::string::size_type quote1 = args.find('"');
            std::string::size_type quote2 = (quote1 != std::string::npos) ? args.find('"', quote1 + 1) : std::string::npos;
            
            std::string user_msg, assistant_msg;
            if (quote1 != std::string::npos && quote2 != std::string::npos) {
                user_msg = args.substr(quote1 + 1, quote2 - quote1 - 1);
                std::string::size_type quote3 = args.find('"', quote2 + 1);
                std::string::size_type quote4 = (quote3 != std::string::npos) ? args.find('"', quote3 + 1) : std::string::npos;
                if (quote3 != std::string::npos && quote4 != std::string::npos) {
                    assistant_msg = args.substr(quote3 + 1, quote4 - quote3 - 1);
                }
            }

            if (user_msg.empty() || assistant_msg.empty()) {
                // Try fallback simple split if they didn't use quotes
                std::string::size_type slash_pos = args.find('|');
                if (slash_pos != std::string::npos) {
                    user_msg = args.substr(0, slash_pos);
                    assistant_msg = args.substr(slash_pos + 1);
                } else {
                    std::cout << "Usage: /turn \"User message\" \"Assistant response\"  OR  /turn user message | assistant response\n";
                    continue;
                }
            }

            std::cout << "Saving turn " << turn_counter << " into episodic memory...\n";
            auto err = monday_engram_commit_turn(db, turn_counter++, "sandbox_session", user_msg.c_str(), assistant_msg.c_str());
            if (err.code == MONDAY_OK) {
                std::cout << "Turn saved successfully!\n";
            } else {
                std::cout << "Failed to save turn: code " << err.code << " (" << err.message << ")\n";
            }
        } else if (line == "/status") {
            std::cout << "Sandbox Database Status:\n"
                      << "  Active session turns tracked: " << turn_counter - 1 << "\n"
                      << "  Vectors path: " << vector_path << "\n";
        } else if (line == "/verify") {
            std::cout << "Verifying cryptographically chained audit log...\n";
            auto err = monday_audit_log_verify();
            if (err.code == MONDAY_OK) {
                std::cout << "✅ Audit log integrity verification SUCCESSFUL! HMAC chains match.\n";
            } else {
                std::cout << "❌ Audit log integrity check FAILED: code " << err.code << " (" << err.message << ")\n";
            }
        } else {
            std::cout << "Unknown command. Type /help for options.\n";
        }
    }

    std::cout << "Exiting sandbox...\n";
    monday_audit_log_shutdown();
    monday_db_close(db);
    monday_allocator_shutdown();
    return 0;
}
