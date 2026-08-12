// monday/security/audit_log.cpp
//
// HMAC-chained append-only audit log.

#include "monday/security/audit_log.h"
#include "monday/security/keystore.h"
#include "monday/core/platform.h"
#include "monday/core/allocator.h"
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef MONDAY_WINDOWS
#include <windows.h>
#include <io.h>
#define fsync(fd) _commit(fd)
#define fileno _fileno
#else
#include <unistd.h>
#include <fcntl.h>
#endif

static char     s_log_path[1024] = {0};
static uint8_t  s_audit_key[32] = {0};
static uint8_t  s_last_hmac[32] = {0};
static uint64_t s_next_seq = 1;
static FILE    *s_log_file = nullptr;

// Helper to convert binary to hex
static void bin_to_hex(const uint8_t *bin, size_t len, char *hex) {
    static const char *hex_chars = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        hex[i*2]   = hex_chars[(bin[i]>>4)&0xf];
        hex[i*2+1] = hex_chars[ bin[i]    &0xf];
    }
    hex[len*2] = '\0';
}

// Helper to convert hex to binary
static int hex_to_bin(const char *hex, uint8_t *bin, size_t max_len) {
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0 || hex_len / 2 > max_len) return -1;
    for (size_t i = 0; i < hex_len / 2; i++) {
        unsigned int val;
        if (sscanf(hex + i*2, "%2x", &val) != 1) return -1;
        bin[i] = (uint8_t)val;
    }
    return 0;
}

// Compute the HMAC of an entry
static void compute_entry_hmac(const uint8_t prev_hmac[32],
                               uint64_t seq,
                               uint64_t ts,
                               const char *action_class,
                               const char *description,
                               uint8_t out_hmac[32]) {
    // Collect all data into a temporary buffer
    size_t action_len = strlen(action_class);
    size_t desc_len = strlen(description);
    size_t total_len = 32 + 8 + 8 + action_len + desc_len;
    
    uint8_t *buf = (uint8_t*)malloc(total_len);
    if (!buf) {
        memset(out_hmac, 0, 32);
        return;
    }

    memcpy(buf, prev_hmac, 32);
    
    // Store big-endian seq and ts
    for (int i = 0; i < 8; i++) {
        buf[32 + i] = (uint8_t)(seq >> ((7 - i) * 8));
        buf[40 + i] = (uint8_t)(ts >> ((7 - i) * 8));
    }
    
    memcpy(buf + 48, action_class, action_len);
    memcpy(buf + 48 + action_len, description, desc_len);

    crypto_auth_hmacsha256(out_hmac, buf, total_len, s_audit_key);
    free(buf);
}

extern "C" {

monday_error_t monday_audit_log_init(const char *log_path) {
    if (!log_path) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null log path");
    }
    strncpy(s_log_path, log_path, sizeof(s_log_path)-1);

    // Initialize libsodium
    if (sodium_init() < 0) {
        return MONDAY_ERROR(MONDAY_ERR_INFERENCE_FAILED, "failed to initialize libsodium");
    }

    // Load or generate audit key
    monday_error_t key_err = monday_keystore_get("monday.audit.key", s_audit_key, 32);
    if (key_err.code == MONDAY_ERR_NOT_FOUND) {
        randombytes_buf(s_audit_key, 32);
        key_err = monday_keystore_set("monday.audit.key", s_audit_key, 32);
        if (key_err.code != MONDAY_OK) {
            return key_err;
        }
    } else if (key_err.code != MONDAY_OK) {
        return key_err;
    }

    // Reset last HMAC and sequence
    memset(s_last_hmac, 0, 32);
    s_next_seq = 1;

    // Verify existing log file and find the last HMAC
    monday_error_t verify_err = monday_audit_log_verify();
    if (verify_err.code != MONDAY_OK && verify_err.code != MONDAY_ERR_NOT_FOUND) {
        return verify_err;
    }

    // Open file in append-only mode
#ifdef MONDAY_WINDOWS
    s_log_file = fopen(s_log_path, "ab+");
#else
    s_log_file = fopen(s_log_path, "ae+");
#endif
    if (!s_log_file) {
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to open audit log file");
    }

    return MONDAY_ERROR(MONDAY_OK, "");
}

monday_error_t monday_audit_log_write(const char *action_class, const char *description) {
    if (!s_log_file) {
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "audit log not initialized");
    }

    // Perform check for rotation (if file exceeds 50 MB)
    long fsize = ftell(s_log_file);
    if (fsize >= 50 * 1024 * 1024) {
        fclose(s_log_file);
        s_log_file = nullptr;

        // Rotate: log.log -> log.log.1, etc. up to 7 generations
        char old_path[2048];
        char new_path[2048];
        for (int i = 6; i >= 1; i--) {
            snprintf(old_path, sizeof(old_path), "%s.%d", s_log_path, i);
            snprintf(new_path, sizeof(new_path), "%s.%d", s_log_path, i+1);
            rename(old_path, new_path);
        }
        snprintf(new_path, sizeof(new_path), "%s.1", s_log_path);
        rename(s_log_path, new_path);

        // Re-open
        s_log_file = fopen(s_log_path, "wb+");
        if (!s_log_file) {
            return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to recreate audit log after rotation");
        }
        memset(s_last_hmac, 0, 32);
        s_next_seq = 1;
    }

    uint64_t seq = s_next_seq;
    uint64_t ts = (uint64_t)time(NULL);

    uint8_t entry_hmac[32];
    compute_entry_hmac(s_last_hmac, seq, ts, action_class, description, entry_hmac);

    char hmac_hex[65];
    bin_to_hex(entry_hmac, 32, hmac_hex);

    // Format: [seq] [ts] [hmac_hex] [action_class] [description]\n
    // Sanitize description and action class to avoid breaking parsing
    char *san_class = strdup(action_class);
    char *san_desc = strdup(description);
    for (size_t i = 0; san_class[i]; i++) {
        if (san_class[i] == '\n' || san_class[i] == '\r' || san_class[i] == '\t') san_class[i] = ' ';
    }
    for (size_t i = 0; san_desc[i]; i++) {
        if (san_desc[i] == '\n' || san_desc[i] == '\r' || san_desc[i] == '\t') san_desc[i] = ' ';
    }

    int written = fprintf(s_log_file, "%lu %lu %s %s %s\n",
                          (unsigned long)seq,
                          (unsigned long)ts,
                          hmac_hex,
                          san_class,
                          san_desc);
    free(san_class);
    free(san_desc);

    if (written < 0) {
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to write to audit log file");
    }

    // Flush and fsync to ensure disk persistence prior to confirming action
    fflush(s_log_file);
    int fd = fileno(s_log_file);
    if (fd >= 0) {
        fsync(fd);
    }

    memcpy(s_last_hmac, entry_hmac, 32);
    s_next_seq++;

    return MONDAY_ERROR(MONDAY_OK, "");
}

monday_error_t monday_audit_log_verify(void) {
    FILE *f = fopen(s_log_path, "r");
    if (!f) {
        // Not found is fine, it means it's a new file
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "audit log file does not exist");
    }

    uint8_t current_last_hmac[32];
    memset(current_last_hmac, 0, 32);
    uint64_t expected_seq = 1;

    char line[4096];
    bool integrity_failed = false;

    while (fgets(line, sizeof(line), f)) {
        uint64_t seq = 0;
        uint64_t ts = 0;
        char hmac_hex[65] = {0};
        char action_class[512] = {0};
        char description[2048] = {0};

        // Parse line: [seq] [ts] [hmac_hex] [action_class] [description...]
        int parsed = sscanf(line, "%lu %lu %64s %511s %[^\n]",
                            (unsigned long*)&seq,
                            (unsigned long*)&ts,
                            hmac_hex,
                            action_class,
                            description);
        
        if (parsed < 4) {
            // Malformed line
            integrity_failed = true;
            break;
        }

        if (seq != expected_seq) {
            integrity_failed = true;
            break;
        }

        uint8_t recorded_hmac[32];
        if (hex_to_bin(hmac_hex, recorded_hmac, 32) != 0) {
            integrity_failed = true;
            break;
        }

        uint8_t computed_hmac[32];
        compute_entry_hmac(current_last_hmac, seq, ts, action_class, description, computed_hmac);

        if (memcmp(recorded_hmac, computed_hmac, 32) != 0) {
            integrity_failed = true;
            break;
        }

        memcpy(current_last_hmac, recorded_hmac, 32);
        expected_seq++;
    }

    fclose(f);

    if (integrity_failed) {
        return MONDAY_ERROR(MONDAY_ERR_DB_INTEGRITY_FAILED, "audit log HMAC chain verification failed");
    }

    // Keep memory cache updated
    memcpy(s_last_hmac, current_last_hmac, 32);
    s_next_seq = expected_seq;

    return MONDAY_ERROR(MONDAY_OK, "");
}

void monday_audit_log_shutdown(void) {
    if (s_log_file) {
        fclose(s_log_file);
        s_log_file = nullptr;
    }
}

} // extern "C"
