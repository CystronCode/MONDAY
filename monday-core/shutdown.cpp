#include "monday-core/shutdown.h"
#include "monday/core/log.h"
#include "monday/core/platform.h"
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef MONDAY_WINDOWS
#include <windows.h>
#endif

// Key registry — populated by monday_shutdown_register_key()
static struct {
    void  *ptr;
    size_t len;
} s_keys[MONDAY_SHUTDOWN_MAX_KEYS];
static int s_key_count = 0;

// Zero all registered key material. Async-signal safe.
static void zero_all_keys(void) {
    for (int i = 0; i < s_key_count; i++) {
        if (s_keys[i].ptr && s_keys[i].len > 0) {
#ifdef MONDAY_WINDOWS
            SecureZeroMemory(s_keys[i].ptr, s_keys[i].len);
#else
            explicit_bzero(s_keys[i].ptr, s_keys[i].len);
#endif
        }
    }
}

static void signal_handler(int sig) {
    (void)sig;
    // Async-signal safe: zero keys, then use _exit (not exit)
    zero_all_keys();
    _exit(0);
}

void monday_shutdown_init(void) {
#ifdef MONDAY_WINDOWS
    signal(SIGTERM, signal_handler);
    signal(SIGABRT, signal_handler);
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;  // restore default after first signal
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
#endif
}

void monday_shutdown_register_key(void *key_ptr, size_t key_len) {
    if (s_key_count < MONDAY_SHUTDOWN_MAX_KEYS) {
        s_keys[s_key_count].ptr = key_ptr;
        s_keys[s_key_count].len = key_len;
        s_key_count++;
    }
}

void monday_shutdown_clean(void) {
    monday_log_write(MONDAY_LOG_SYSTEM, "monday-core shutdown");
    monday_log_close();
    zero_all_keys();
    exit(0);
}

extern "C" void monday_loop_shutdown_c(void);
