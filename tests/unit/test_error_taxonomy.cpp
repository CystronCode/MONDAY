// tests/unit/test_error_taxonomy.cpp
#include <catch2/catch_test_macros.hpp>
#include "monday/core/errors.h"
#include "monday/core/result.h"
#include <vector>
#include <set>
#include <string>

TEST_CASE("Error taxonomy codes and macros", "[errors]") {
    SECTION("MONDAY_OK is 0") {
        REQUIRE(MONDAY_OK == 0);
    }

    SECTION("All error codes are distinct") {
        std::vector<monday_error_code_t> codes = {
            MONDAY_OK,
            MONDAY_ERR_INVALID_ARG,
            MONDAY_ERR_OUT_OF_MEMORY,
            MONDAY_ERR_NOT_FOUND,
            MONDAY_ERR_ALREADY_EXISTS,
            MONDAY_ERR_TIMEOUT,
            MONDAY_ERR_CANCELLED,
            MONDAY_ERR_NOT_SUPPORTED,
            MONDAY_ERR_PERMISSION_DENIED,
            MONDAY_ERR_MODEL_LOAD_FAILED,
            MONDAY_ERR_MODEL_HASH_MISMATCH,
            MONDAY_ERR_INFERENCE_FAILED,
            MONDAY_ERR_CONTEXT_OVERFLOW,
            MONDAY_ERR_ENGINE_BUSY,
            MONDAY_ERR_DB_OPEN_FAILED,
            MONDAY_ERR_DB_CORRUPT,
            MONDAY_ERR_DB_WRONG_KEY,
            MONDAY_ERR_DB_MIGRATION_FAILED,
            MONDAY_ERR_DB_INTEGRITY_FAILED,
            MONDAY_ERR_IPC_AUTH_FAILED,
            MONDAY_ERR_IPC_REPLAY,
            MONDAY_ERR_IPC_SCHEMA_MISMATCH,
            MONDAY_ERR_IPC_PEER_DIED,
            MONDAY_ERR_CONSENT_DENIED,
            MONDAY_ERR_PLUGIN_UNTRUSTED,
            MONDAY_ERR_AUTOMATION_BLOCKED,
            MONDAY_ERR_MANIFEST_INVALID,
            MONDAY_ERR_BB_KEY_NOT_OWNED,
            MONDAY_ERR_BB_TYPE_MISMATCH,
            MONDAY_ERR_BB_SIZE_EXCEEDED,
            MONDAY_ERR_BB_STALE_KEY,
            MONDAY_ERR_ARENA_FULL,
            MONDAY_ERR_ARENA_RESET_BUSY,
            MONDAY_ERR_AUDIO_DEVICE_LOST,
            MONDAY_ERR_AUDIO_OVERRUN
        };

        std::set<int> unique_codes;
        for (auto code : codes) {
            unique_codes.insert(static_cast<int>(code));
        }

        REQUIRE(unique_codes.size() == codes.size());
    }

    SECTION("MONDAY_ERROR macro populates code, message, file, and line") {
        int expected_line = __LINE__ + 1;
        monday_error_t err = MONDAY_ERROR(MONDAY_ERR_TIMEOUT, "timed out");

        REQUIRE(err.code == MONDAY_ERR_TIMEOUT);
        REQUIRE(std::string(err.message) == "timed out");
        REQUIRE(err.file != nullptr);
        REQUIRE(err.line == expected_line);
    }
}

TEST_CASE("Monday Result template behavior", "[result]") {
    SECTION("Result holding success value is truthy and value matches") {
        monday::Result<int> res = 42;
        REQUIRE(res.has_value());
        REQUIRE(static_cast<bool>(res));
        REQUIRE(res.value() == 42);
    }

    SECTION("Result holding error is falsy and matches error code") {
        monday_error_t err_cancelled = MONDAY_ERROR(MONDAY_ERR_CANCELLED, "operation cancelled");
        monday::Result<int> res = tl::make_unexpected(err_cancelled);

        REQUIRE_FALSE(static_cast<bool>(res));
        REQUIRE(res.error().code == MONDAY_ERR_CANCELLED);
        REQUIRE(std::string(res.error().message) == "operation cancelled");
    }
}
