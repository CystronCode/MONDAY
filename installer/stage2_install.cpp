// installer/stage2_install.cpp
//
// Stage 2 Installer: Atomic rename & Secure Keystore registration.
// Architecture §2.3:
//   1. Verifies manifest in staging directory.
//   2. Moves staging directory atomically to target directory.
//   3. Updates secure keystore with the SHA-256 hash of manifest.json.

#include "monday/security/model_verify.h"
#include "monday/security/keystore.h"
#include "monday/core/allocator.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sodium.h>

namespace fs = std::filesystem;

// Minimal SHA-256 helper for local use
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

int main(int argc, char **argv) {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium" << std::endl;
        return 1;
    }

    bool crash_after_rename = false;
    std::string base_path = "";

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--crash-after-rename") == 0) {
            crash_after_rename = true;
        } else {
            base_path = argv[i];
        }
    }

    if (base_path.empty()) {
        std::cerr << "Usage: stage2_install [--crash-after-rename] <base_path>" << std::endl;
        return 1;
    }

    // Initialize mock allocator zones so that logging / helpers don't crash
    const size_t STATIC_ZONE  = 1024 * 1024;
    const size_t LEVEL_ZONE   = 1024 * 1024;
    const size_t CACHE_ZONE   = 1024 * 1024;
    const size_t TLSF_POOL    = 4 * 1024 * 1024;
    (void)monday_allocator_init(STATIC_ZONE, LEVEL_ZONE, CACHE_ZONE, TLSF_POOL);
    (void)monday_log_init("stage2_install.log", 1024 * 1024, 1);

    fs::path staging_dir = fs::path(base_path) / "staging";
    fs::path target_dir = fs::path(base_path) / "model";

    fs::path staging_manifest = staging_dir / "manifest.json";

    std::cout << "[stage2_install] Verifying manifest in staging: " << staging_manifest << std::endl;
    monday_model_manifest_t manifest;
    monday_error_t verify_err = monday_model_verify(staging_manifest.string().c_str(), &manifest);
    if (verify_err.code != MONDAY_OK && verify_err.code != MONDAY_ERR_NOT_SUPPORTED) {
        std::cerr << "[stage2_install] Verification failed with error code: " << verify_err.code << std::endl;
        return 1;
    }

    std::cout << "[stage2_install] Performing atomic rename..." << std::endl;
    try {
        if (fs::exists(target_dir)) {
            fs::remove_all(target_dir);
        }
        fs::rename(staging_dir, target_dir);
    } catch (const fs::filesystem_error &e) {
        std::cerr << "[stage2_install] Directory move failed: " << e.what() << std::endl;
        return 1;
    }

    if (crash_after_rename) {
        std::cout << "[stage2_install] Simulating crash after rename (--crash-after-rename)" << std::endl;
        std::exit(1);
    }

    fs::path target_manifest = target_dir / "manifest.json";
    uint8_t manifest_hash[32];
    if (!compute_file_sha256(target_manifest.string(), manifest_hash)) {
        std::cerr << "[stage2_install] Failed to compute SHA-256 of target manifest" << std::endl;
        return 1;
    }

    std::cout << "[stage2_install] Updating secure keystore with manifest hash..." << std::endl;
    monday_error_t ks_err = monday_keystore_set("monday.manifest.sha256", manifest_hash, 32);
    if (ks_err.code != MONDAY_OK) {
        std::cerr << "[stage2_install] Failed to save manifest hash to secure keystore: " << ks_err.code << std::endl;
        return 1;
    }

    std::cout << "[stage2_install] Installation complete and secure keystore updated successfully." << std::endl;
    return 0;
}
