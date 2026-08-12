#include "monday-core/main_loop.h"
#include "monday-core/shutdown.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"
#include "monday/core/platform.h"
#include "monday/inference/cpuid_dispatch.h"
#include "monday/inference/IInferenceEngine.h"
#include "monday/inference/engine_selector.h"
#include "monday/security/model_verify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#ifdef MONDAY_WINDOWS
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

using namespace monday;

const size_t STATIC_ZONE  = 16 * 1024 * 1024;   // 16 MB
const size_t LEVEL_ZONE   = 32 * 1024 * 1024;   // 32 MB
const size_t CACHE_ZONE   = 16 * 1024 * 1024;   // 16 MB
const size_t TLSF_POOL    = 64 * 1024 * 1024;   // 64 MB

static size_t get_free_ram_mb(void) {
#ifdef MONDAY_WINDOWS
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    return (size_t)(memInfo.ullAvailPhys / (1024 * 1024));
#else
    FILE* file = fopen("/proc/meminfo", "r");
    if (!file) return 4096; // fallback
    char line[256];
    size_t mem_avail_kb = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line, "MemAvailable: %zu kB", &mem_avail_kb);
            break;
        }
    }
    fclose(file);
    if (mem_avail_kb > 0) return mem_avail_kb / 1024;
    return 4096; // fallback
#endif
}

// Simple strip function
static char* strip(char* s) {
    while(isspace(*s)) s++;
    if(*s == 0) return s;
    char* end = s + strlen(s) - 1;
    while(end > s && isspace(*end)) end--;
    end[1] = '\0';
    return s;
}

int main(int argc, char** argv) {
    bool text_only = false;
    bool benchmark = false;
    char config_path[1024] = {0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--text-only") == 0) {
            text_only = true;
        } else if (strcmp(argv[i], "--benchmark-inference") == 0) {
            benchmark = true;
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            strncpy(config_path, argv[++i], sizeof(config_path) - 1);
        }
    }

    if (strlen(config_path) == 0) {
        char* env_cfg = getenv("MONDAY_CONFIG");
        if (env_cfg) {
            strncpy(config_path, env_cfg, sizeof(config_path) - 1);
        } else {
#ifdef MONDAY_WINDOWS
            char appdata[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
                snprintf(config_path, sizeof(config_path), "%s\\monday\\config.toml", appdata);
            }
#else
            char* home = getenv("HOME");
            if (home) {
                snprintf(config_path, sizeof(config_path), "%s/.config/monday/config.toml", home);
            }
#endif
        }
    }

    EnginePreference pref_engine = EnginePreference::AUTO;
    char model_path_cfg[1024] = {0};
    MondayLoopConfig loop_cfg;
    loop_cfg.text_only = text_only;
    loop_cfg.benchmark_mode = benchmark;
    loop_cfg.temperature = 0.7f;
    loop_cfg.top_p = 0.9f;
    loop_cfg.max_new_tokens = 512;

    FILE* cfg_f = fopen(config_path, "r");
    if (cfg_f) {
        char line[512];
        bool in_inference = false;
        while (fgets(line, sizeof(line), cfg_f)) {
            char* s = strip(line);
            if (s[0] == '[' && strstr(s, "[inference]")) {
                in_inference = true;
                continue;
            }
            if (s[0] == '[') {
                in_inference = false;
                continue;
            }
            if (!in_inference || s[0] == '#' || s[0] == '\0') continue;
            
            char* eq = strchr(s, '=');
            if (!eq) continue;
            *eq = '\0';
            char* key = strip(s);
            char* val = strip(eq + 1);
            
            if (val[0] == '"') {
                val++;
                char* endq = strrchr(val, '"');
                if (endq) *endq = '\0';
            }
            
            if (strcmp(key, "engine") == 0) {
                if (strcmp(val, "bitnet") == 0) pref_engine = EnginePreference::BITNET;
                else if (strcmp(val, "llama") == 0) pref_engine = EnginePreference::LLAMA;
                else pref_engine = EnginePreference::AUTO;
            } else if (strcmp(key, "model_path") == 0) {
                strncpy(model_path_cfg, val, sizeof(model_path_cfg) - 1);
            } else if (strcmp(key, "temperature") == 0) {
                loop_cfg.temperature = (float)atof(val);
            } else if (strcmp(key, "top_p") == 0) {
                loop_cfg.top_p = (float)atof(val);
            } else if (strcmp(key, "max_new_tokens") == 0) {
                loop_cfg.max_new_tokens = (uint32_t)atoi(val);
            }
        }
        fclose(cfg_f);
    }

    monday_shutdown_init();
    
    int log_rc = monday_log_init("monday.log", 50*1024*1024, 7);
    if (log_rc != 0) {
        fprintf(stderr, "Failed to init log\n");
        exit(1);
    }

    monday_error_t err = monday_allocator_init(STATIC_ZONE, LEVEL_ZONE, CACHE_ZONE, TLSF_POOL);
    if (err.code != MONDAY_OK) {
        monday_log_write(MONDAY_LOG_SYSTEM, "allocator init failed");
        exit(1);
    }
    
    monday_cpuid_dispatch_init();
    
    size_t free_ram_mb = get_free_ram_mb();
    
    std::unique_ptr<IInferenceEngine> engine = monday_select_engine(pref_engine, free_ram_mb);
    if (!engine) {
        monday_log_write(MONDAY_LOG_SYSTEM, "engine selection failed");
        exit(1);
    }

    if (strlen(model_path_cfg) == 0) {
        strncpy(model_path_cfg, "model.bin", sizeof(model_path_cfg) - 1);
    }

    char manifest_path[1024];
    strncpy(manifest_path, model_path_cfg, sizeof(manifest_path) - 1);
    char* last_sep = strrchr(manifest_path, '/');
    if (!last_sep) last_sep = strrchr(manifest_path, '\\');
    if (last_sep) {
        strcpy(last_sep + 1, "manifest.json");
    } else {
        strcpy(manifest_path, "manifest.json");
    }

    monday_model_manifest_t manifest;
    err = monday_model_verify(manifest_path, &manifest);
    if (err.code != MONDAY_OK && err.code != MONDAY_ERR_NOT_SUPPORTED) {
        monday_log_write(MONDAY_LOG_SYSTEM, "model verify failed");
        exit(1);
    }

    auto load_res = engine->load_model(model_path_cfg, manifest_path);
    if (!load_res) {
        monday_log_write(MONDAY_LOG_SYSTEM, "engine load_model failed");
        exit(1);
    }

    monday_loop_init(engine.get(), loop_cfg);
    monday_loop_run();

    engine->unload_model();
    monday_shutdown_clean();

    return 0;
}
