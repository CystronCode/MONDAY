// monday/security/audit_log.h
//
// HMAC-chained append-only audit log.
// Architecture §3.7: log rotation, fsync, and tamper-evident chaining.

#pragma once

#include "monday/core/errors.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the audit log.
// log_path: absolute path to the log file.
monday_error_t monday_audit_log_init(const char *log_path);

// Append a tool execution record to the log.
// Invokes fsync before returning MONDAY_OK.
monday_error_t monday_audit_log_write(const char *action_class, const char *description);

// Verify the integrity of the log file by validating the HMAC chain.
monday_error_t monday_audit_log_verify(void);

// Shut down the audit log.
void monday_audit_log_shutdown(void);

#ifdef __cplusplus
}
#endif
