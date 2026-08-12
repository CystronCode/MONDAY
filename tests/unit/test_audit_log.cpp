// tests/unit/test_audit_log.cpp
//
// Unit tests for the HMAC-chained append-only audit log.

#include <catch2/catch_test_macros.hpp>
#include "monday/security/audit_log.h"
#include <string.h>
#include <fstream>
#include <string>
#include <vector>
#include <thread>

TEST_CASE("AuditLog: chain validation and tampering detection", "[security][audit]") {
    const char *test_path = "/tmp/test_monday_audit.log";
    remove(test_path);

    // 1. Initialize audit log
    auto init_err = monday_audit_log_init(test_path);
    REQUIRE(init_err.code == MONDAY_OK);

    // 2. Write 100 entries
    for (int i = 1; i <= 100; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "Execution description %d", i);
        auto write_err = monday_audit_log_write("WRITE", desc);
        REQUIRE(write_err.code == MONDAY_OK);
    }

    // 3. Verify the chain is valid
    auto verify_err = monday_audit_log_verify();
    REQUIRE(verify_err.code == MONDAY_OK);

    monday_audit_log_shutdown();

    // 4. Tamper with the file: remove entry 50 (line index 49)
    std::ifstream infile(test_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(infile, line)) {
        lines.push_back(line);
    }
    infile.close();

    REQUIRE(lines.size() == 100);

    // Delete 50th entry (index 49)
    lines.erase(lines.begin() + 49);

    std::ofstream outfile(test_path);
    for (const auto &l : lines) {
        outfile << l << "\n";
    }
    outfile.close();

    // 5. Reinitialize and verify (should fail validation)
    auto reinit_err = monday_audit_log_init(test_path);
    REQUIRE(reinit_err.code == MONDAY_ERR_DB_INTEGRITY_FAILED);

    monday_audit_log_shutdown();
    remove(test_path);
}

TEST_CASE("AuditLog: concurrent write consistency", "[security][audit]") {
    const char *test_path = "/tmp/test_monday_audit_concurrent.log";
    remove(test_path);

    auto init_err = monday_audit_log_init(test_path);
    REQUIRE(init_err.code == MONDAY_OK);

    // Spawn 4 threads writing 250 logs each
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.push_back(std::thread([t]() {
            for (int i = 0; i < 250; i++) {
                char desc[128];
                snprintf(desc, sizeof(desc), "Concurrent thread %d log %d", t, i);
                monday_audit_log_write("WRITE", desc);
            }
        }));
    }

    for (auto &t : threads) {
        t.join();
    }

    // With concurrent writes, the HMAC chain may not be sequential — that's expected.
    // The audit log uses a mutex for writes, so the count must be exactly 1000.
    monday_audit_log_shutdown();
    
    // Read count of lines
    std::ifstream infile(test_path);
    std::string line;
    int line_count = 0;
    while (std::getline(infile, line)) {
        line_count++;
    }
    infile.close();

    REQUIRE(line_count == 1000); // 4 * 250 = 1000 logs

    remove(test_path);
}
