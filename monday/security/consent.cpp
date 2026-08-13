// monday/security/consent.cpp
#include "monday/security/consent.h"
#include "monday/core/log.h"
#include <sqlite3.h>
#include <ctime>
#include <algorithm>
#include <cctype>

namespace monday {

static std::string trim_string(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string action_class_to_string(ActionClass ac) {
    switch (ac) {
        case ActionClass::READ_ONLY:   return "READ_ONLY";
        case ActionClass::WRITE:       return "WRITE";
        case ActionClass::DESTRUCTIVE: return "DESTRUCTIVE";
        case ActionClass::SHELL:       return "SHELL";
        case ActionClass::NETWORK:     return "NETWORK";
    }
    return "UNKNOWN";
}

Result<ActionClass> action_class_from_string(const std::string& str) {
    if (str == "READ_ONLY")   return ActionClass::READ_ONLY;
    if (str == "WRITE")       return ActionClass::WRITE;
    if (str == "DESTRUCTIVE") return ActionClass::DESTRUCTIVE;
    if (str == "SHELL")       return ActionClass::SHELL;
    if (str == "NETWORK")     return ActionClass::NETWORK;
    return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "Invalid ActionClass string"));
}

bool ConsentManager::verify_cherry_input(const std::string& input) {
    std::string trimmed = trim_string(input);
    std::string lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
    return (lower == "cherry");
}

Result<void> ConsentManager::init(sqlite3* db, const std::string& session_id) {
    db_ = db;
    session_id_ = session_id;

    if (db_) {
        const char* schema_sql =
            "CREATE TABLE IF NOT EXISTS permission_grants ("
            "    id           INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    action_class TEXT    NOT NULL,"
            "    target_scope TEXT    NOT NULL,"
            "    granted_at   INTEGER NOT NULL,"
            "    expires_at   INTEGER NOT NULL,"
            "    session_id   TEXT    NOT NULL"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_grants_target ON permission_grants(action_class, target_scope);";

        char* err_msg = nullptr;
        int rc = sqlite3_exec(db_, schema_sql, nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string err = err_msg ? err_msg : "sqlite3_exec failed";
            sqlite3_free(err_msg);
            return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, err.c_str()));
        }

        // Purge expired grants on initialization
        (void)purge_expired_grants();
    }
    return {};
}

Result<void> ConsentManager::purge_expired_grants() {
    if (!db_) return {};
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    const char* sql = "DELETE FROM permission_grants WHERE expires_at <= ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "Failed to prepare purge SQL"));
    }
    sqlite3_bind_int64(stmt, 1, now);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "Failed to purge expired grants"));
    }
    return {};
}

Result<bool> ConsentManager::check_active_grant(ActionClass action_class, const std::string& target_scope) {
    if (!db_) return false;
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    std::string ac_str = action_class_to_string(action_class);

    const char* sql =
        "SELECT id FROM permission_grants "
        "WHERE action_class = ? AND target_scope = ? AND expires_at > ? "
        "LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "Failed to prepare check grant SQL"));
    }
    sqlite3_bind_text(stmt, 1, ac_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, target_scope.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, now);

    int step_rc = sqlite3_step(stmt);
    bool found = (step_rc == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return found;
}

Result<void> ConsentManager::store_permission_grant(ActionClass action_class, const std::string& target_scope) {
    if (!db_) return {};
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    int64_t expires_at = now + 86400; // 24 Hours TTL
    std::string ac_str = action_class_to_string(action_class);

    const char* sql =
        "INSERT INTO permission_grants (action_class, target_scope, granted_at, expires_at, session_id) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "Failed to prepare store grant SQL"));
    }
    sqlite3_bind_text(stmt, 1, ac_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, target_scope.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, now);
    sqlite3_bind_int64(stmt, 4, expires_at);
    sqlite3_bind_text(stmt, 5, session_id_.c_str(), -1, SQLITE_TRANSIENT);

    int step_rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "Failed to insert permission grant"));
    }
    return {};
}

Result<bool> ConsentManager::request_consent(
    ActionClass action_class,
    const std::string& target_scope,
    const std::string& description,
    bool is_cherry_scoped,
    const std::string& user_cherry_input
) {
    // READ_ONLY actions require no confirmation dialog
    if (action_class == ActionClass::READ_ONLY) {
        return true;
    }

    // Handle Cherry confirmation if requested
    if (is_cherry_scoped) {
        std::string typed_input = user_cherry_input;
        if (typed_input.empty() && cherry_ui_handler_) {
            typed_input = cherry_ui_handler_(description);
        }
        if (!verify_cherry_input(typed_input)) {
            monday_log_write(MONDAY_LOG_SYSTEM, "Cherry confirmation failed or mismatch");
            return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_CONSENT_DENIED, "Cherry confirmation failed or mismatch"));
        }
        // Cherry confirmed
        return true;
    }

    // Check for existing active grant in DB (only WRITE grants persist)
    if (action_class == ActionClass::WRITE) {
        auto active_res = check_active_grant(action_class, target_scope);
        if (active_res.has_value() && active_res.value()) {
            return true; // Auto-granted via existing active TTL grant
        }
    }

    // Solicit consent from UI handler
    bool approved = false;
    if (ui_handler_) {
        approved = ui_handler_(action_class, target_scope, description);
    } else {
        // Default headless stance: auto-approve WRITE/READ_ONLY in non-interactive tests, deny DESTRUCTIVE/SHELL/NETWORK without UI
        approved = (action_class == ActionClass::WRITE);
    }

    if (!approved) {
        return tl::unexpected(MONDAY_ERROR(MONDAY_ERR_CONSENT_DENIED, "User denied consent"));
    }

    // Store grant in DB IF action class is WRITE (DESTRUCTIVE, SHELL, NETWORK are NEVER saved)
    if (action_class == ActionClass::WRITE) {
        auto store_res = store_permission_grant(action_class, target_scope);
        if (!store_res.has_value()) {
            return tl::unexpected(store_res.error());
        }
    }

    return true;
}

} // namespace monday
