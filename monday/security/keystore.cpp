// monday/security/keystore.cpp
//
// Secure Keystore implementation with local file fallback and permissions check.

#include "monday/security/keystore.h"
#include "monday/core/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MONDAY_WINDOWS
#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <errno.h>
#endif

// Helper to sanitize name for filesystem safety
static void sanitize_name(const char *name, char *out, size_t max_len) {
    size_t i = 0;
    for (; name[i] && i < max_len - 1; i++) {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_') {
            out[i] = c;
        } else {
            out[i] = '_';
        }
    }
    out[i] = '\0';
}

static bool get_keystore_dir(char *out_path, size_t max_len) {
#ifdef MONDAY_WINDOWS
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) != S_OK) {
        return false;
    }
    snprintf(out_path, max_len, "%s\\monday", appdata);
    CreateDirectoryA(out_path, NULL);
    return true;
#else
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) return false;

    snprintf(out_path, max_len, "%s/.config", home);
    mkdir(out_path, 0700);
    snprintf(out_path, max_len, "%s/.config/monday", home);
    mkdir(out_path, 0700);
    return true;
#endif
}

extern "C" {

monday_error_t monday_keystore_set(const char *name, const uint8_t *data, size_t len) {
    if (!name || !data || len == 0) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid keystore set arguments");
    }

    char dir[1024];
    if (!get_keystore_dir(dir, sizeof(dir))) {
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "failed to get keystore directory");
    }

    char clean_name[256];
    sanitize_name(name, clean_name, sizeof(clean_name));

    char file_path[2048];
#ifdef MONDAY_WINDOWS
    snprintf(file_path, sizeof(file_path), "%s\\%s.bin", dir, clean_name);
#else
    snprintf(file_path, sizeof(file_path), "%s/%s.bin", dir, clean_name);
#endif

#ifdef MONDAY_WINDOWS
    // On Windows, use DPAPI to encrypt the key before saving
    DATA_BLOB data_in;
    DATA_BLOB data_out;
    data_in.pbData = (BYTE*)data;
    data_in.cbData = (DWORD)len;

    if (!CryptProtectData(&data_in, L"Monday Key", NULL, NULL, NULL, 0, &data_out)) {
        return MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "DPAPI encryption failed");
    }

    FILE *f = fopen(file_path, "wb");
    if (!f) {
        LocalFree(data_out.pbData);
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to open keystore file for writing");
    }
    fwrite(data_out.pbData, 1, data_out.cbData, f);
    fclose(f);
    LocalFree(data_out.pbData);
#else
    // On Linux/WSL, write file with strict user-only permissions (0600)
    int fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        return MONDAY_ERROR(MONDAY_ERR_DB_OPEN_FAILED, "failed to open keystore file for writing");
    }
    
    ssize_t written = write(fd, data, len);
    close(fd);

    if (written != (ssize_t)len) {
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to write complete key to keystore file");
    }
#endif

    return MONDAY_ERROR(MONDAY_OK, "");
}

monday_error_t monday_keystore_get(const char *name, uint8_t *out_data, size_t len) {
    if (!name || !out_data || len == 0) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "invalid keystore get arguments");
    }

    char dir[1024];
    if (!get_keystore_dir(dir, sizeof(dir))) {
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "failed to get keystore directory");
    }

    char clean_name[256];
    sanitize_name(name, clean_name, sizeof(clean_name));

    char file_path[2048];
#ifdef MONDAY_WINDOWS
    snprintf(file_path, sizeof(file_path), "%s\\%s.bin", dir, clean_name);
#else
    snprintf(file_path, sizeof(file_path), "%s/%s.bin", dir, clean_name);
#endif

    FILE *f = fopen(file_path, "rb");
    if (!f) {
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "key not found in keystore");
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(f);
        return MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "keystore file is empty");
    }

    uint8_t *buf = (uint8_t*)malloc(fsize);
    if (!buf) {
        fclose(f);
        return MONDAY_ERROR(MONDAY_ERR_OUT_OF_MEMORY, "failed to allocate memory for reading key");
    }

    size_t nread = fread(buf, 1, fsize, f);
    fclose(f);

    if (nread != (size_t)fsize) {
        free(buf);
        return MONDAY_ERROR(MONDAY_ERR_DB_CORRUPT, "failed to read keystore file");
    }

#ifdef MONDAY_WINDOWS
    // On Windows, decrypt using DPAPI
    DATA_BLOB data_in;
    DATA_BLOB data_out;
    data_in.pbData = buf;
    data_in.cbData = (DWORD)fsize;

    if (!CryptUnprotectData(&data_in, NULL, NULL, NULL, NULL, 0, &data_out)) {
        free(buf);
        return MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "DPAPI decryption failed");
    }

    if (data_out.cbData != len) {
        LocalFree(data_out.pbData);
        free(buf);
        return MONDAY_ERROR(MONDAY_ERR_BB_SIZE_EXCEEDED, "decrypted key size mismatch");
    }

    memcpy(out_data, data_out.pbData, len);
    LocalFree(data_out.pbData);
    free(buf);
#else
    // On Linux/WSL, direct read
    if ((size_t)fsize != len) {
        free(buf);
        return MONDAY_ERROR(MONDAY_ERR_BB_SIZE_EXCEEDED, "key size mismatch");
    }
    memcpy(out_data, buf, len);
    free(buf);
#endif

    return MONDAY_ERROR(MONDAY_OK, "");
}

monday_error_t monday_keystore_delete(const char *name) {
    if (!name) return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null name");

    char dir[1024];
    if (!get_keystore_dir(dir, sizeof(dir))) {
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "failed to get keystore directory");
    }

    char clean_name[256];
    sanitize_name(name, clean_name, sizeof(clean_name));

    char file_path[2048];
#ifdef MONDAY_WINDOWS
    snprintf(file_path, sizeof(file_path), "%s\\%s.bin", dir, clean_name);
#else
    snprintf(file_path, sizeof(file_path), "%s/%s.bin", dir, clean_name);
#endif

#ifdef MONDAY_WINDOWS
    if (DeleteFileA(file_path) == 0) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "key not found");
        }
        return MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "failed to delete key");
    }
#else
    if (unlink(file_path) != 0) {
        if (errno == ENOENT) {
            return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "key not found");
        }
        return MONDAY_ERROR(MONDAY_ERR_PERMISSION_DENIED, "failed to delete key");
    }
#endif

    return MONDAY_ERROR(MONDAY_OK, "");
}

} // extern "C"
