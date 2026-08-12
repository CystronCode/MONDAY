// tests/integration/test_inference_engine.cpp
//
// Phase 1 integration test: IInferenceEngine full contract verification.
// Tests all assertions from the build guide Phase 1 TESTING PROTOCOL.
//
// Uses a synthetic test fixture:
//   - A temp directory with a small model file and a valid manifest.json
//   - SHA-256 is computed on-the-fly so the manifest always matches.
//   - A tampered variant (one byte flipped) is used to test HASH_MISMATCH.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "monday/inference/IInferenceEngine.h"
#include "monday/inference/backends/bitnet_engine.h"
#include "monday/inference/backends/llama_engine.h"
#include "monday/inference/engine_selector.h"
#include "monday/inference/cpuid_dispatch.h"
#include "monday/core/allocator.h"
#include "monday/core/errors.h"
#include "monday/core/log.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <atomic>
#include <fstream>
#include <sstream>

#ifdef MONDAY_WINDOWS
#include <windows.h>
#include <io.h>
#define snprintf _snprintf
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

using namespace monday;

// ── SHA-256 helper (copied from model_verify.cpp scope) ──────────────────────
// Re-declared here for test fixture use.

static std::string sha256_hex_of_file(const std::string &path) {
    // Compute SHA-256 via a small inline implementation
    // (duplicate from model_verify.cpp for test isolation)

    struct sha256_ctx { uint8_t data[64]; uint32_t datalen; uint64_t bitlen; uint32_t state[8]; };
    static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    auto rotr=[](uint32_t x,int n){return(x>>n)|(x<<(32-n));};
    auto transform=[&](sha256_ctx *ctx,const uint8_t *d){
        uint32_t a,b,c,de,f,g,h,t1,t2,m[64];
        for(int i=0,j=0;i<16;++i,j+=4) m[i]=((uint32_t)d[j]<<24)|((uint32_t)d[j+1]<<16)|((uint32_t)d[j+2]<<8)|(uint32_t)d[j+3];
        for(int i=16;i<64;++i) m[i]=(rotr(m[i-2],17)^rotr(m[i-2],19)^(m[i-2]>>10))+m[i-7]+(rotr(m[i-15],7)^rotr(m[i-15],18)^(m[i-15]>>3))+m[i-16];
        a=ctx->state[0];b=ctx->state[1];c=ctx->state[2];de=ctx->state[3];f=ctx->state[4];g=ctx->state[5];h=ctx->state[6];uint32_t hh=ctx->state[7];
        for(int i=0;i<64;++i){t1=hh+(rotr(f,6)^rotr(f,11)^rotr(f,25))+((f&g)^(~f&h))+k[i]+m[i];t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));hh=h;h=g;g=f;f=de+(uint32_t)t1;de=c;c=b;b=a;a=t1+t2;}
        ctx->state[0]+=a;ctx->state[1]+=b;ctx->state[2]+=c;ctx->state[3]+=de;ctx->state[4]+=f;ctx->state[5]+=g;ctx->state[6]+=h;ctx->state[7]+=hh;
    };
    sha256_ctx ctx;
    ctx.datalen=0;ctx.bitlen=0;ctx.state[0]=0x6a09e667;ctx.state[1]=0xbb67ae85;ctx.state[2]=0x3c6ef372;ctx.state[3]=0xa54ff53a;ctx.state[4]=0x510e527f;ctx.state[5]=0x9b05688c;ctx.state[6]=0x1f83d9ab;ctx.state[7]=0x5be0cd19;
    FILE *f=fopen(path.c_str(),"rb");
    if(!f) return "";
    uint8_t buf[4096]; size_t n;
    while((n=fread(buf,1,sizeof(buf),f))>0){for(size_t i=0;i<n;++i){ctx.data[ctx.datalen++]=buf[i];if(ctx.datalen==64){transform(&ctx,ctx.data);ctx.bitlen+=512;ctx.datalen=0;}}}
    fclose(f);
    uint32_t i=ctx.datalen;
    if(ctx.datalen<56){ctx.data[i++]=0x80;while(i<56)ctx.data[i++]=0;}else{ctx.data[i++]=0x80;while(i<64)ctx.data[i++]=0;transform(&ctx,ctx.data);memset(ctx.data,0,56);}
    ctx.bitlen+=ctx.datalen*8;
    for(int j=7;j>=0;j--){ctx.data[56+(7-j)]=(uint8_t)(ctx.bitlen>>(j*8));}
    transform(&ctx,ctx.data);
    uint8_t hash[32];
    for(int j=0;j<4;++j){hash[j]=(ctx.state[0]>>(24-j*8))&0xff;hash[j+4]=(ctx.state[1]>>(24-j*8))&0xff;hash[j+8]=(ctx.state[2]>>(24-j*8))&0xff;hash[j+12]=(ctx.state[3]>>(24-j*8))&0xff;hash[j+16]=(ctx.state[4]>>(24-j*8))&0xff;hash[j+20]=(ctx.state[5]>>(24-j*8))&0xff;hash[j+24]=(ctx.state[6]>>(24-j*8))&0xff;hash[j+28]=(ctx.state[7]>>(24-j*8))&0xff;}
    static const char hc[]="0123456789abcdef";
    std::string out;out.resize(64);
    for(int j=0;j<32;j++){out[j*2]=hc[(hash[j]>>4)&0xf];out[j*2+1]=hc[hash[j]&0xf];}
    return out;
}

// ── Test Fixture ──────────────────────────────────────────────────────────────

struct TestModelFixture {
    std::string dir;
    std::string model_path;
    std::string manifest_path;
    std::string tampered_model_path;

    TestModelFixture() {
        // Create a temp directory
#ifdef MONDAY_WINDOWS
        char tmp[MAX_PATH];
        GetTempPathA(MAX_PATH, tmp);
        dir = std::string(tmp) + "monday_test_model\\";
        _mkdir(dir.c_str());
#else
        dir = "/tmp/monday_test_model/";
        mkdir(dir.c_str(), 0700);
#endif
        model_path    = dir + "model.bin";
        manifest_path = dir + "manifest.json";
        tampered_model_path = dir + "model_tampered.bin";

        // Write a small synthetic model file (4 KB of known bytes)
        {
            FILE *f = fopen(model_path.c_str(), "wb");
            REQUIRE(f != nullptr);
            for (int i = 0; i < 4096; i++) fputc(i & 0xFF, f);
            fclose(f);
        }

        // Write a tampered copy (flip byte 0)
        {
            FILE *src = fopen(model_path.c_str(), "rb");
            FILE *dst = fopen(tampered_model_path.c_str(), "wb");
            REQUIRE(src != nullptr);
            REQUIRE(dst != nullptr);
            int ch = fgetc(src);
            fputc(ch ^ 0xFF, dst);  // flip first byte
            while ((ch = fgetc(src)) != EOF) fputc(ch, dst);
            fclose(src);
            fclose(dst);
        }

        // Compute SHA-256 of the GOOD model
        std::string sha = sha256_hex_of_file(model_path);
        REQUIRE(!sha.empty());

        // Write manifest.json
        // Architecture §2.3 manifest format:
        {
            FILE *f = fopen(manifest_path.c_str(), "w");
            REQUIRE(f != nullptr);
            fprintf(f, "{\n");
            fprintf(f, "  \"version\": 1,\n");
            fprintf(f, "  \"model_id\": \"test-model-v1\",\n");
            fprintf(f, "  \"files\": [\n");
            fprintf(f, "    { \"path\": \"model.bin\", \"sha256\": \"%s\", \"size_bytes\": 4096 }\n",
                    sha.c_str());
            fprintf(f, "  ],\n");
            // Stub signature (all zeros base64url — will be treated as NOT_SUPPORTED in stub build)
            fprintf(f, "  \"ed25519_signature\": \"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"\n");
            fprintf(f, "}\n");
            fclose(f);
        }
    }

    ~TestModelFixture() {
        remove(model_path.c_str());
        remove(tampered_model_path.c_str());
        remove(manifest_path.c_str());
        // dir removal: leave it — OK for tests
    }
};

// ── Global allocator setup ────────────────────────────────────────────────────

static bool g_allocator_inited = false;

static void ensure_allocator(void) {
    if (!g_allocator_inited) {
        monday_cpuid_dispatch_init();
        monday_error_t e = monday_allocator_init(
            4*1024*1024, 8*1024*1024, 4*1024*1024, 8*1024*1024);
        REQUIRE(e.code == MONDAY_OK);
        g_allocator_inited = true;
    }
}

// ── Test: load_model() on valid path + valid manifest → MONDAY_OK ─────────────

TEST_CASE("BitnetEngine: load_model valid manifest succeeds", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;
    BitnetEngine engine;
    auto r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    INFO("Error msg: " << (r ? "none" : r.error().message));
    REQUIRE(r.has_value());
    engine.unload_model();
}

// ── Test: load_model() tampered model → MONDAY_ERR_MODEL_HASH_MISMATCH ────────

TEST_CASE("BitnetEngine: load_model tampered model returns HASH_MISMATCH", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;

    // Write a manifest pointing to the good hash but load the tampered file
    // Strategy: swap model.bin with the tampered one after writing the manifest
    std::string good_model = fix.model_path;

    // Create a manifest that points to model.bin (which we will corrupt)
    // Then rename tampered_model to model.bin
#ifdef MONDAY_WINDOWS
    _unlink(good_model.c_str());
    rename(fix.tampered_model_path.c_str(), good_model.c_str());
#else
    unlink(good_model.c_str());
    rename(fix.tampered_model_path.c_str(), good_model.c_str());
#endif

    BitnetEngine engine;
    auto r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(!r.has_value());
    REQUIRE(r.error().code == MONDAY_ERR_MODEL_HASH_MISMATCH);
}

// ── Test: load_model() missing manifest → MONDAY_ERR_MANIFEST_INVALID ────────

TEST_CASE("BitnetEngine: load_model missing manifest returns MANIFEST_INVALID", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;
    BitnetEngine engine;
    auto r = engine.load_model(fix.model_path.c_str(), "/nonexistent/path/manifest.json");
    REQUIRE(!r.has_value());
    REQUIRE(r.error().code == MONDAY_ERR_MANIFEST_INVALID);
}

// ── Test: Blocking inference → result.text non-null, caller can monday_free() ─

TEST_CASE("BitnetEngine: blocking inference returns valid text", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;
    BitnetEngine engine;

    auto load_r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(load_r.has_value());

    InferenceRequest req = {};
    req.prompt         = "Hello, Monday.";
    req.max_new_tokens = 32;
    req.temperature    = 0.7f;
    req.top_p          = 0.9f;
    req.on_token       = nullptr;  // blocking mode
    req.append_to_kv   = false;

    auto r = engine.run_inference(req);
    REQUIRE(r.has_value());
    REQUIRE(r->text != nullptr);
    REQUIRE(strlen(r->text) > 0);
    REQUIRE(r->token_count > 0);

    // Caller MUST monday_free() the text
    monday_free(r->text);
    // No ASan error after free = test passes

    engine.unload_model();
}

// ── Test: Streaming inference → on_token called ≥ 1 time, result.text is null ─

TEST_CASE("BitnetEngine: streaming inference calls on_token and text is null", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;
    BitnetEngine engine;

    auto load_r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(load_r.has_value());

    std::atomic<int> token_calls{0};
    std::string accumulated;

    InferenceRequest req = {};
    req.prompt         = "What is the capital of France?";
    req.max_new_tokens = 64;
    req.temperature    = 0.0f;  // greedy
    req.top_p          = 0.0f;
    req.append_to_kv   = false;
    req.on_token       = [&](int32_t tok_id, const char *tok_text) -> bool {
        if (tok_id == -1) return false;  // error signal
        token_calls.fetch_add(1);
        if (tok_text) accumulated += tok_text;
        return true;
    };

    auto r = engine.run_inference(req);
    REQUIRE(r.has_value());
    REQUIRE(r->text == nullptr);  // streaming: text is null
    REQUIRE(token_calls.load() >= 1);

    engine.unload_model();
}

// ── Test: Cancellation — on_token returns false after 5 tokens ───────────────

TEST_CASE("BitnetEngine: cancellation after 5 tokens stops inference", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;
    BitnetEngine engine;

    auto load_r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(load_r.has_value());

    std::atomic<int> token_count{0};
    std::atomic<int> after_cancel_calls{0};
    bool cancelled = false;

    InferenceRequest req = {};
    req.prompt         = "Tell me a very long story about everything.";
    req.max_new_tokens = 512;
    req.temperature    = 0.7f;
    req.top_p          = 0.9f;
    req.append_to_kv   = false;
    req.on_token       = [&](int32_t tok_id, const char *tok_text) -> bool {
        (void)tok_text;
        if (tok_id == -1) return false;
        if (cancelled) {
            after_cancel_calls.fetch_add(1);
            return false;
        }
        int n = token_count.fetch_add(1) + 1;
        if (n >= 5) {
            cancelled = true;
            return false;  // cancel
        }
        return true;
    };

    auto r = engine.run_inference(req);
    REQUIRE(r.has_value());
    // After cancellation: no further callbacks
    REQUIRE(after_cancel_calls.load() == 0);
    REQUIRE(token_count.load() <= 5);

    engine.unload_model();
}

// ── Test: get_kv_cache() generation counter ───────────────────────────────────

TEST_CASE("BitnetEngine: get_kv_cache generation counter", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;
    BitnetEngine engine;

    auto load_r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(load_r.has_value());

    KVSnapshot snap0 = engine.get_kv_cache();
    REQUIRE(snap0.generation == 0);  // before first inference

    // Run blocking inference
    InferenceRequest req = {};
    req.prompt         = "Hello.";
    req.max_new_tokens = 8;
    req.on_token       = nullptr;
    req.append_to_kv   = false;
    auto r = engine.run_inference(req);
    REQUIRE(r.has_value());
    monday_free(r->text);

    KVSnapshot snap1 = engine.get_kv_cache();
    REQUIRE(snap1.generation == 1);  // after first inference

    engine.unload_model();
}

// ── Test: reset_context() increments generation, kv_size_bytes == 0 ───────────

TEST_CASE("BitnetEngine: reset_context increments generation", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;
    BitnetEngine engine;

    auto load_r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(load_r.has_value());

    KVSnapshot before = engine.get_kv_cache();
    uint32_t gen_before = before.generation;

    engine.reset_context();

    KVSnapshot after = engine.get_kv_cache();
    REQUIRE(after.generation == gen_before + 1);
    REQUIRE(after.kv_size_bytes == 0);

    engine.unload_model();
}

// ── Test: get_capabilities() returns STREAMING | KV_CACHE ────────────────────

TEST_CASE("BitnetEngine: get_capabilities returns required flags", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;
    BitnetEngine engine;

    auto load_r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(load_r.has_value());

    InferenceCapability caps = engine.get_capabilities();
    REQUIRE((caps & InferenceCapability::STREAMING));
    REQUIRE((caps & InferenceCapability::KV_CACHE));

    // Must report the correct SIMD tier
    monday_cpuid_dispatch_init();
    if (g_cpu_features == MONDAY_CPU_AVX2) {
        REQUIRE((caps & InferenceCapability::AVX2));
    } else if (g_cpu_features == MONDAY_CPU_SSE4_2) {
        REQUIRE((caps & InferenceCapability::SSE4_2));
    }

    engine.unload_model();
}

// ── Test: RSS after unload_model() within 5 MB of pre-load RSS ───────────────

TEST_CASE("BitnetEngine: unload_model does not leak RSS", "[inference][bitnet]") {
    ensure_allocator();
    TestModelFixture fix;

    // Read RSS before load
    auto read_rss_kb = []() -> long {
#ifdef MONDAY_WINDOWS
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        return (long)(pmc.WorkingSetSize / 1024);
#else
        FILE *f = fopen("/proc/self/status", "r");
        if (!f) return 0;
        char line[256];
        long rss = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line + 6, " %ld", &rss);
                break;
            }
        }
        fclose(f);
        return rss;
#endif
    };

    BitnetEngine engine;
    long rss_before = read_rss_kb();

    auto load_r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(load_r.has_value());

    engine.unload_model();
    long rss_after = read_rss_kb();

    // Must be within 5 MB = 5120 KB
    long diff = std::abs(rss_after - rss_before);
    INFO("RSS before: " << rss_before << " KB, after: " << rss_after << " KB, diff: " << diff << " KB");
    REQUIRE(diff < 5120);
}

// ── LlamaEngine: same contract ────────────────────────────────────────────────

TEST_CASE("LlamaEngine: load_model valid manifest succeeds", "[inference][llama]") {
    ensure_allocator();
    TestModelFixture fix;
    LlamaEngine engine;
    auto r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(r.has_value());
    REQUIRE((engine.get_capabilities() & InferenceCapability::GRACEFUL_DEGRADED));
    engine.unload_model();
}

TEST_CASE("LlamaEngine: blocking inference returns valid text", "[inference][llama]") {
    ensure_allocator();
    TestModelFixture fix;
    LlamaEngine engine;
    auto load_r = engine.load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(load_r.has_value());

    InferenceRequest req = {};
    req.prompt         = "Hello.";
    req.max_new_tokens = 16;
    req.on_token       = nullptr;
    req.append_to_kv   = false;

    auto r = engine.run_inference(req);
    REQUIRE(r.has_value());
    REQUIRE(r->text != nullptr);
    monday_free(r->text);
    engine.unload_model();
}

// ── Engine selector: picks correct backend ────────────────────────────────────

TEST_CASE("EngineSelector: selects llama on constrained RAM", "[inference][selector]") {
    ensure_allocator();
    monday_cpuid_dispatch_init();
    // Less than 2048 MB free → must select Llama (GRACEFUL_DEGRADED)
    auto engine = monday_select_engine(EnginePreference::AUTO, 1024);
    REQUIRE(engine != nullptr);
    TestModelFixture fix;
    auto r = engine->load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(r.has_value());
    REQUIRE((engine->get_capabilities() & InferenceCapability::GRACEFUL_DEGRADED));
    engine->unload_model();
}

TEST_CASE("EngineSelector: explicit LLAMA preference selects llama", "[inference][selector]") {
    ensure_allocator();
    monday_cpuid_dispatch_init();
    auto engine = monday_select_engine(EnginePreference::LLAMA, 8192);
    REQUIRE(engine != nullptr);
    TestModelFixture fix;
    auto r = engine->load_model(fix.model_path.c_str(), fix.manifest_path.c_str());
    REQUIRE(r.has_value());
    REQUIRE((engine->get_capabilities() & InferenceCapability::GRACEFUL_DEGRADED));
    engine->unload_model();
}
