// monday/automation/automation_blocklist.c
#include "monday/automation/automation_blocklist.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

static const char* kBlockedBinaries[] = {
    "consent.exe",
    "polkit",
    "sudo",
    "doas",
    "keepassxc",
    "1password",
    "bitwarden",
    "gnome-keyring",
    "kwallet",
    "kwalletd5",
    "kwalletd6",
    NULL
};

static const char* kBlockedWindowClasses[] = {
    "Polkit",
    "KeePassXC",
    "1Password",
    "Bitwarden",
    "Gnome-keyring-prompt",
    "CredentialUIBroker",
    NULL
};

static bool contains_case_insensitive(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    size_t h_len = strlen(haystack);
    size_t n_len = strlen(needle);
    if (n_len > h_len) return false;

    for (size_t i = 0; i <= h_len - n_len; ++i) {
        bool match = true;
        for (size_t j = 0; j < n_len; ++j) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

bool monday_is_process_blocked(const char* binary_path) {
    if (!binary_path) return true;
    for (size_t i = 0; kBlockedBinaries[i] != NULL; ++i) {
        if (contains_case_insensitive(binary_path, kBlockedBinaries[i])) {
            return true;
        }
    }
    return false;
}

bool monday_is_window_blocked(const char* window_class) {
    if (!window_class) return true;
    for (size_t i = 0; kBlockedWindowClasses[i] != NULL; ++i) {
        if (contains_case_insensitive(window_class, kBlockedWindowClasses[i])) {
            return true;
        }
    }
    return false;
}
