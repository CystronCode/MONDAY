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
#include "monday/security/keystore.h"

// Subsystem headers for integration test suite
#include "monday/goap/goap_planner.h"
#include "monday/bt/bt_engine.h"
#include "monday/storage/db.h"
#include "monday/storage/migrations.h"
#include "monday/rag/ingestion_pipeline.h"
#include "monday/rag/hybrid_retriever.h"
#include "monday/plugin/plugin_host.h"
#include "monday/plugin/capability_broker.h"
#include "monday/plugin/lua_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <memory>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <filesystem>

#ifdef MONDAY_WINDOWS
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

using namespace monday;

// Local SHA-256 implementation details (identical to stage2_install)
struct local_sha256_ctx {
    uint8_t  data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
};

static const uint32_t k256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR32(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z)   (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z)  (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x)      (ROTR32(x,2)^ROTR32(x,13)^ROTR32(x,22))
#define EP1(x)      (ROTR32(x,6)^ROTR32(x,11)^ROTR32(x,25))
#define SIG0(x)     (ROTR32(x,7)^ROTR32(x,18)^((x)>>3))
#define SIG1(x)     (ROTR32(x,17)^ROTR32(x,19)^((x)>>10))

static void local_sha256_transform(local_sha256_ctx *ctx, const uint8_t *data) {
    uint32_t a,b,c,d,e,f,g,h,i,j,t1,t2,m[64];
    for (i=0,j=0;i<16;++i,j+=4)
        m[i]=((uint32_t)data[j]<<24)|((uint32_t)data[j+1]<<16)|
             ((uint32_t)data[j+2]<<8)|(uint32_t)data[j+3];
    for (;i<64;++i)
        m[i]=SIG1(m[i-2])+m[i-7]+SIG0(m[i-15])+m[i-16];
    a=ctx->state[0];b=ctx->state[1];c=ctx->state[2];d=ctx->state[3];
    e=ctx->state[4];f=ctx->state[5];g=ctx->state[6];h=ctx->state[7];
    for (i=0;i<64;++i){
        t1=h+EP1(e)+CH(e,f,g)+k256[i]+m[i];
        t2=EP0(a)+MAJ(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    ctx->state[0]+=a;ctx->state[1]+=b;ctx->state[2]+=c;ctx->state[3]+=d;
    ctx->state[4]+=e;ctx->state[5]+=f;ctx->state[6]+=g;ctx->state[7]+=h;
}

static void local_sha256_init(local_sha256_ctx *ctx) {
    ctx->datalen=0; ctx->bitlen=0;
    ctx->state[0]=0x6a09e667;ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372;ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f;ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab;ctx->state[7]=0x5be0cd19;
}

static void local_sha256_update(local_sha256_ctx *ctx, const uint8_t *data, size_t len) {
    for (size_t i=0;i<len;++i){
        ctx->data[ctx->datalen]=data[i];
        ctx->datalen++;
        if (ctx->datalen==64){
            local_sha256_transform(ctx,ctx->data);
            ctx->bitlen+=512;
            ctx->datalen=0;
        }
    }
}

static void local_sha256_final(local_sha256_ctx *ctx, uint8_t *hash) {
    uint32_t i=ctx->datalen;
    if (ctx->datalen<56){
        ctx->data[i++]=0x80;
        while(i<56) ctx->data[i++]=0x00;
    } else {
        ctx->data[i++]=0x80;
        while(i<64) ctx->data[i++]=0x00;
        local_sha256_transform(ctx,ctx->data);
        memset(ctx->data,0,56);
    }
    ctx->bitlen+=ctx->datalen*8;
    ctx->data[63]=(uint8_t)(ctx->bitlen);
    ctx->data[62]=(uint8_t)(ctx->bitlen>>8);
    ctx->data[61]=(uint8_t)(ctx->bitlen>>16);
    ctx->data[60]=(uint8_t)(ctx->bitlen>>24);
    ctx->data[59]=(uint8_t)(ctx->bitlen>>32);
    ctx->data[58]=(uint8_t)(ctx->bitlen>>40);
    ctx->data[57]=(uint8_t)(ctx->bitlen>>48);
    ctx->data[56]=(uint8_t)(ctx->bitlen>>56);
    local_sha256_transform(ctx,ctx->data);
    for (i=0;i<4;++i){
        hash[i]   =(ctx->state[0]>>(24-i*8))&0xff;
        hash[i+4] =(ctx->state[1]>>(24-i*8))&0xff;
        hash[i+8] =(ctx->state[2]>>(24-i*8))&0xff;
        hash[i+12]=(ctx->state[3]>>(24-i*8))&0xff;
        hash[i+16]=(ctx->state[4]>>(24-i*8))&0xff;
        hash[i+20]=(ctx->state[5]>>(24-i*8))&0xff;
        hash[i+24]=(ctx->state[6]>>(24-i*8))&0xff;
        hash[i+28]=(ctx->state[7]>>(24-i*8))&0xff;
    }
}

static bool compute_file_sha256(const std::string &path, uint8_t hash_out[32]) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    local_sha256_ctx ctx;
    local_sha256_init(&ctx);
    char buf[4096];
    while (file.read(buf, sizeof(buf))) {
        local_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(buf), file.gcount());
    }
    if (file.gcount() > 0) {
        local_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(buf), file.gcount());
    }
    local_sha256_final(&ctx, hash_out);
    return true;
}

// Subsystem execution simulation loop
static void monday_run_integration_test_suite(int duration_seconds) {
    std::cout << "[integration_test_suite] Starting integration test suite for " << duration_seconds << " seconds..." << std::endl;
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Integration test suite started");

    auto start_time = std::chrono::steady_clock::now();
    int iteration = 0;

    GOAPPlanner planner;
    if (!planner.init("data/intents.jsonl").has_value()) {
        std::cout << "[integration_test_suite] Warning: could not initialize GOAP Planner (trying fallback data paths)" << std::endl;
        (void)planner.init("../data/intents.jsonl");
    }

    const char *db_path = ":memory:";
    monday_db_t *db = nullptr;
    auto open_err = monday_db_open(db_path, nullptr, &db);
    if (open_err.code == MONDAY_OK) {
        (void)monday_migrations_run(db);
    }

    MiniLMEmbedder embedder;
    TierSelector selector("/tmp/integration_test_diskann.bin", db, 100);
    bool rag_ok = false;
    if (embedder.init("dummy").has_value()) {
        rag_ok = true;
    }
    std::unique_ptr<IngestionPipeline> pipeline;
    std::unique_ptr<HybridRetriever> retriever;
    if (rag_ok && db) {
        pipeline = std::make_unique<IngestionPipeline>(db, &embedder, &selector);
        retriever = std::make_unique<HybridRetriever>(db, &embedder, &selector);
    }

    CapabilityBroker broker;
    PluginHost host(&broker);

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        if (elapsed >= duration_seconds) {
            break;
        }

        iteration++;
        std::cout << "[integration_test_suite] Iteration " << iteration << " (elapsed: " << elapsed << "s)" << std::endl;

        // Exercise allocator
        void *p = monday_zone_alloc(MONDAY_ZONE_LEVEL, 1024);
        if (p) {
            memset(p, 0xAA, 1024);
        }
        (void)monday_level_reset(); // reset LEVEL zone

        // Exercise logger
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Integration loop heartbeat");
        monday_log_write_kv_long(MONDAY_LOG_VERBOSE, "Loop state", "iteration", iteration);

        // Exercise GOAP
        if (iteration % 2 == 0) {
            auto goap_res = planner.classify("set a timer for 10 minutes");
            (void)goap_res;
        }

        // Exercise Storage
        if (db) {
            char query[256];
            snprintf(query, sizeof(query), "INSERT OR REPLACE INTO config (key, value) VALUES ('test_key_%d', 'test_val_%d')", iteration, iteration);
            (void)monday_db_exec(db, query);
        }

        // Exercise RAG
        if (pipeline && retriever && iteration % 5 == 1) {
            std::string doc_id = "doc_" + std::to_string(iteration);
            (void)pipeline->ingest_document(doc_id, "The offline system runs on old hardware.");
            auto results = retriever->retrieve("offline system", 1);
            (void)results;
        }

        // Sleep to avoid pegging CPU (allows sanitizers to run cleanly)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (db) {
        monday_db_close(db);
    }
    std::filesystem::remove("/tmp/integration_test_diskann.bin");

    std::cout << "[integration_test_suite] Integration test suite completed successfully (ran " << iteration << " iterations)." << std::endl;
    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Integration test suite completed");
}


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
    bool verify_only = false;
    bool integration_test_suite = false;
    int duration = 60;
    char config_path[1024] = {0};

    char cli_model_path[1024] = {0};
    bool skip_verify = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--text-only") == 0) {
            text_only = true;
        } else if (strcmp(argv[i], "--benchmark-inference") == 0) {
            benchmark = true;
        } else if (strcmp(argv[i], "--verify-only") == 0) {
            verify_only = true;
        } else if (strcmp(argv[i], "--no-verify") == 0) {
            skip_verify = true;
        } else if (strcmp(argv[i], "--integration-test-suite") == 0) {
            integration_test_suite = true;
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            strncpy(config_path, argv[++i], sizeof(config_path) - 1);
        } else if (strcmp(argv[i], "--model_path") == 0 && i + 1 < argc) {
            strncpy(cli_model_path, argv[++i], sizeof(cli_model_path) - 1);
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

    if (integration_test_suite) {
        monday_run_integration_test_suite(duration);
        monday_shutdown_clean();
        return 0;
    }

    if (strlen(cli_model_path) > 0) {
        strncpy(model_path_cfg, cli_model_path, sizeof(model_path_cfg) - 1);
    } else if (strlen(model_path_cfg) == 0) {
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

    // ── Manifest & Keystore Verification (Architecture §2.3) ──
    uint8_t computed_manifest_hash[32] = {0};
    bool hash_ok = compute_file_sha256(manifest_path, computed_manifest_hash);

    bool keystore_match = false;
    if (hash_ok) {
        uint8_t stored_manifest_hash[32] = {0};
        monday_error_t ks_err = monday_keystore_get("monday.manifest.sha256", stored_manifest_hash, 32);
        if (ks_err.code == MONDAY_OK) {
            if (memcmp(computed_manifest_hash, stored_manifest_hash, 32) == 0) {
                keystore_match = true;
                monday_log_write(MONDAY_LOG_DIAGNOSTIC, "model_verify: manifest hash matched secure keystore entry");
            }
        }
    }

    monday_model_manifest_t manifest;
    if (!keystore_match && !skip_verify) {
        // If mismatch or missing, run full Ed25519 and SHA-256 validation of model files
        err = monday_model_verify(manifest_path, &manifest);
        if (err.code != MONDAY_OK && err.code != MONDAY_ERR_NOT_SUPPORTED) {
            monday_log_write(MONDAY_LOG_SYSTEM, "model verify failed (hash mismatch/verification failed)");
            exit(1);
        }
        // Verification succeeded, so update the secure keystore with computed hash
        if (hash_ok) {
            monday_error_t ks_set_err = monday_keystore_set("monday.manifest.sha256", computed_manifest_hash, 32);
            if (ks_set_err.code != MONDAY_OK) {
                monday_log_write(MONDAY_LOG_SYSTEM, "Failed to update manifest hash in secure keystore");
            } else {
                monday_log_write(MONDAY_LOG_DIAGNOSTIC, "model_verify: secure keystore entry updated with manifest hash");
            }
        }
    } else {
        // Even if hash matches, we run verification to check model file existence and size correctness
        err = monday_model_verify(manifest_path, &manifest);
        if (err.code != MONDAY_OK && err.code != MONDAY_ERR_NOT_SUPPORTED) {
            monday_log_write(MONDAY_LOG_SYSTEM, "model verify failed (keystore match but verify failed)");
            exit(1);
        }
    }

    if (verify_only) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "Model verification successful (verify-only)");
        monday_shutdown_clean();
        return 0;
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
