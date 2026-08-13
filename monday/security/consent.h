// monday/security/consent.h
#pragma once

#include "monday/core/result.h"
#include "monday/core/errors.h"
#include <string>
#include <functional>
#include <cstdint>

struct sqlite3;

namespace monday {

enum class ActionClass {
    READ_ONLY,
    WRITE,
    DESTRUCTIVE,
    SHELL,
    NETWORK
};

std::string action_class_to_string(ActionClass ac);
Result<ActionClass> action_class_from_string(const std::string& str);

// Mockable UI callback for consent dialogs in tests/standalone runs
using ConsentUIHandler = std::function<bool(ActionClass action_class, const std::string& target_scope, const std::string& description)>;
using CherryUIHandler = std::function<std::string(const std::string& description)>;

class ConsentManager {
public:
    ConsentManager() = default;
    ~ConsentManager() = default;

    Result<void> init(sqlite3* db, const std::string& session_id);

    // Checks consent for the given action class and target scope.
    // For WRITE: checks DB for valid unexpired grant (24h TTL). If missing, prompts UI, then saves to DB if approved.
    // For DESTRUCTIVE, SHELL, NETWORK: prompts UI every time, NEVER saves to DB.
    // For Cherry-scoped operations: presents typed prompt requiring exact "cherry" input.
    Result<bool> request_consent(
        ActionClass action_class,
        const std::string& target_scope,
        const std::string& description,
        bool is_cherry_scoped = false,
        const std::string& user_cherry_input = ""
    );

    // Purges expired grants (expires_at <= current_time) from SQLCipher DB
    Result<void> purge_expired_grants();

    // Verifies whether user-provided typed input satisfies "cherry" confirmation
    static bool verify_cherry_input(const std::string& input);

    // Set custom UI handlers (used by tests and trusted helper IPC bridge)
    void set_ui_handler(ConsentUIHandler handler) { ui_handler_ = std::move(handler); }
    void set_cherry_ui_handler(CherryUIHandler handler) { cherry_ui_handler_ = std::move(handler); }

private:
    Result<bool> check_active_grant(ActionClass action_class, const std::string& target_scope);
    Result<void> store_permission_grant(ActionClass action_class, const std::string& target_scope);

    sqlite3* db_{nullptr};
    std::string session_id_;
    ConsentUIHandler ui_handler_;
    CherryUIHandler cherry_ui_handler_;
};

} // namespace monday
