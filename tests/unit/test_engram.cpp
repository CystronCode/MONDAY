// tests/unit/test_engram.cpp
//
// Unit tests for the ENGRAM Memory Engine.

#include <catch2/catch_test_macros.hpp>
#include "monday/engram/engram.h"
#include "monday/engram/episodic_store.h"
#include "monday/storage/db.h"
#include "monday/storage/migrations.h"
#include "monday/security/keystore.h"
#include "monday/core/allocator.h"
#include <string.h>

TEST_CASE("Engram: episodic store HMAC verification and quarantine", "[engram]") {
    const char *db_path = "/tmp/test_monday_engram.db";
    remove(db_path);

    // Initialise memory allocator (required before any zone alloc)
    monday_allocator_init(1024*1024, 512*1024, 512*1024, 2*1024*1024);

    // Initialise components
    REQUIRE(monday_engram_init().code == MONDAY_OK);

    monday_db_t *db = nullptr;
    auto open_err = monday_db_open(db_path, nullptr, &db);
    REQUIRE(open_err.code == MONDAY_OK);

    auto mig_err = monday_migrations_run(db);
    REQUIRE(mig_err.code == MONDAY_OK);

    // 1. Insert a clean episodic record
    monday_episodic_record_t record;
    record.turn_id = 1;
    strcpy(record.session_id, "session_abc");
    record.summary = (char*)"Key decisions: built Phase 3.";
    record.conversation = (char*)"User: how is it going?\nMonday: great!";
    record.timestamp = 100000;

    auto ins_err = monday_episodic_store_insert(db, &record);
    REQUIRE(ins_err.code == MONDAY_OK);

    // 2. Retrieve the record
    monday_episodic_record_t retrieved = {};
    auto get_err = monday_episodic_store_get(db, 1, &retrieved);
    REQUIRE(get_err.code == MONDAY_OK);
    REQUIRE(retrieved.turn_id == 1);
    REQUIRE(strcmp(retrieved.session_id, "session_abc") == 0);
    REQUIRE(strcmp(retrieved.summary, "Key decisions: built Phase 3.") == 0);
    REQUIRE(strcmp(retrieved.conversation, "User: how is it going?\nMonday: great!") == 0);

    monday_episodic_record_free(&retrieved);

    // 3. Tamper with the database content directly using SQL exec
    auto exec_err = monday_db_exec(db, "UPDATE episodic_memory SET conversation = 'User: hacked!' WHERE turn_id = 1;");
    REQUIRE(exec_err.code == MONDAY_OK);

    // 4. Retrieve tampered record (should fail HMAC verification and be quarantined)
    monday_episodic_record_t tampered_ret = {};
    auto tampered_err = monday_episodic_store_get(db, 1, &tampered_ret);
    REQUIRE(tampered_err.code == MONDAY_ERR_DB_INTEGRITY_FAILED);

    monday_db_close(db);
    remove(db_path);
}
