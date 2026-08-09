// monday/core/errors.h
#pragma once

typedef enum monday_error_code {
    MONDAY_OK                      = 0,
    // General (1–8)
    MONDAY_ERR_INVALID_ARG         = 1,
    MONDAY_ERR_OUT_OF_MEMORY       = 2,
    MONDAY_ERR_NOT_FOUND           = 3,
    MONDAY_ERR_ALREADY_EXISTS      = 4,
    MONDAY_ERR_TIMEOUT             = 5,
    MONDAY_ERR_CANCELLED           = 6,
    MONDAY_ERR_NOT_SUPPORTED       = 7,
    MONDAY_ERR_PERMISSION_DENIED   = 8,
    // Inference (100–104)
    MONDAY_ERR_MODEL_LOAD_FAILED   = 100,
    MONDAY_ERR_MODEL_HASH_MISMATCH = 101,
    MONDAY_ERR_INFERENCE_FAILED    = 102,
    MONDAY_ERR_CONTEXT_OVERFLOW    = 103,
    MONDAY_ERR_ENGINE_BUSY         = 104,
    // Storage (200–204)
    MONDAY_ERR_DB_OPEN_FAILED      = 200,
    MONDAY_ERR_DB_CORRUPT          = 201,
    MONDAY_ERR_DB_WRONG_KEY        = 202,
    MONDAY_ERR_DB_MIGRATION_FAILED = 203,
    MONDAY_ERR_DB_INTEGRITY_FAILED = 204,
    // IPC (300–303)
    MONDAY_ERR_IPC_AUTH_FAILED     = 300,
    MONDAY_ERR_IPC_REPLAY          = 301,
    MONDAY_ERR_IPC_SCHEMA_MISMATCH = 302,
    MONDAY_ERR_IPC_PEER_DIED       = 303,
    // Security / Consent (400–403)
    MONDAY_ERR_CONSENT_DENIED      = 400,
    MONDAY_ERR_PLUGIN_UNTRUSTED    = 401,
    MONDAY_ERR_AUTOMATION_BLOCKED  = 402,
    MONDAY_ERR_MANIFEST_INVALID    = 403,
    // Blackboard (500–503)
    MONDAY_ERR_BB_KEY_NOT_OWNED    = 500,
    MONDAY_ERR_BB_TYPE_MISMATCH    = 501,
    MONDAY_ERR_BB_SIZE_EXCEEDED    = 502,
    MONDAY_ERR_BB_STALE_KEY        = 503,
    // Allocator (600–601)
    MONDAY_ERR_ARENA_FULL          = 600,
    MONDAY_ERR_ARENA_RESET_BUSY    = 601,
    // Audio (700–701)
    MONDAY_ERR_AUDIO_DEVICE_LOST   = 700,
    MONDAY_ERR_AUDIO_OVERRUN       = 701,
} monday_error_code_t;

typedef struct {
    monday_error_code_t  code;
    const char          *message;  /* static string; never heap-allocated */
    const char          *file;     /* __FILE__ */
    int                  line;     /* __LINE__ */
} monday_error_t;

#ifdef __cplusplus
#define MONDAY_ERROR(code, msg) \
    (monday_error_t{ (code), (msg), __FILE__, __LINE__ })
#else
#define MONDAY_ERROR(code, msg) \
    ((monday_error_t){ (code), (msg), __FILE__, __LINE__ })
#endif
