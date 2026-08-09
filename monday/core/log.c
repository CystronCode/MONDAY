// monday/core/log.c
#include "monday/core/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

static FILE          *g_log_file       = NULL;
static char           g_log_path[4096] = {0};
static size_t         g_max_bytes      = 0;
static int            g_generations    = 0;
static size_t         g_current_bytes  = 0;
static pthread_mutex_t g_mutex         = PTHREAD_MUTEX_INITIALIZER;

static const char *level_name(monday_log_level_t l) {
    switch (l) {
        case MONDAY_LOG_SYSTEM:     return "SYSTEM";
        case MONDAY_LOG_DIAGNOSTIC: return "DIAGNOSTIC";
        case MONDAY_LOG_VERBOSE:    return "VERBOSE";
        case MONDAY_LOG_SENSITIVE:  return "SENSITIVE";
        default:                    return "UNKNOWN";
    }
}

// Rename existing files: .6 → dropped, .5 → .6, ... .1 → .2, base → .1
static void rotate(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    // Drop the oldest generation
    char oldest[4200];
    snprintf(oldest, sizeof(oldest), "%s.%d", g_log_path, g_generations);
    remove(oldest);

    // Shift everything down
    for (int i = g_generations - 1; i >= 1; --i) {
        char src[4200], dst[4200];
        snprintf(src, sizeof(src), "%s.%d", g_log_path, i);
        snprintf(dst, sizeof(dst), "%s.%d", g_log_path, i + 1);
        rename(src, dst);
    }

    // Rename the live file to .1
    char first[4200];
    snprintf(first, sizeof(first), "%s.1", g_log_path);
    rename(g_log_path, first);

    // Open a fresh file
    g_log_file = fopen(g_log_path, "a");
    g_current_bytes = 0;
}

int monday_log_init(const char *path, size_t max_size_bytes, int generations) {
    pthread_mutex_lock(&g_mutex);

    strncpy(g_log_path, path, sizeof(g_log_path) - 1);
    g_max_bytes   = max_size_bytes;
    g_generations = generations;

    g_log_file = fopen(path, "a");
    if (!g_log_file) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    // Measure current file size
    struct stat st;
    if (stat(path, &st) == 0) {
        g_current_bytes = (size_t)st.st_size;
    }

    pthread_mutex_unlock(&g_mutex);
    return 0;
}

static void write_line(monday_log_level_t level, const char *msg,
                        const char *extra_key, long extra_val,
                        int has_extra) {
    if (!g_log_file) return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long ts_sec = (long)ts.tv_sec;

    char line[8192];
    int  len;

    if (has_extra) {
        len = snprintf(line, sizeof(line),
            "{\"ts\":%ld,\"level\":\"%s\",\"msg\":\"%s\",\"%s\":%ld}\n",
            ts_sec, level_name(level), msg, extra_key, extra_val);
    } else {
        len = snprintf(line, sizeof(line),
            "{\"ts\":%ld,\"level\":\"%s\",\"msg\":\"%s\"}\n",
            ts_sec, level_name(level), msg);
    }

    if (len <= 0) return;

    // Rotate if needed
    if (g_current_bytes + (size_t)len > g_max_bytes) {
        rotate();
        if (!g_log_file) return;
    }

    fwrite(line, 1, (size_t)len, g_log_file);
    fflush(g_log_file);
    g_current_bytes += (size_t)len;
}

void monday_log_write(monday_log_level_t level, const char *msg) {
    pthread_mutex_lock(&g_mutex);
    write_line(level, msg, NULL, 0, 0);
    pthread_mutex_unlock(&g_mutex);
}

void monday_log_write_kv_long(monday_log_level_t level,
                               const char *msg,
                               const char *key,
                               long value) {
    pthread_mutex_lock(&g_mutex);
    write_line(level, msg, key, value, 1);
    pthread_mutex_unlock(&g_mutex);
}

void monday_log_close(void) {
    pthread_mutex_lock(&g_mutex);
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    pthread_mutex_unlock(&g_mutex);
}
