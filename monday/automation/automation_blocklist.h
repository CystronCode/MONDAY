// monday/automation/automation_blocklist.h
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns true if the specified process binary path is on the hardcoded safety blocklist
// (UAC dialogs, polkit, sudo, doas, KeePassXC, 1Password, Bitwarden, gnome-keyring, kwallet).
bool monday_is_process_blocked(const char* binary_path);

// Returns true if the window class / title indicates a blocked security process.
bool monday_is_window_blocked(const char* window_class);

#ifdef __cplusplus
}
#endif
