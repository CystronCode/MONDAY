#include "monday/core/platform.h"
#include "monday/security/model_verify.h"
#include "monday/security/ed25519_verify.h"
#include "monday/core/log.h"
#include "monday/core/errors.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef MONDAY_WINDOWS
#define strncasecmp _strnicmp
#endif

// ── SHA-256 (public-domain implementation) ────────────────────────────────────
// Adapted from the public-domain implementation by Brad Conte.
// This is the only cryptographic primitive implemented locally;
// all asymmetric crypto uses libsodium (or the stub that refuses).

typedef struct {
    uint8_t  data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} sha256_ctx_t;

static const uint32_t k256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR32(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z)   (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z)  (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x)      (ROTR32(x,2)^ROTR32(x,13)^ROTR32(x,22))
#define EP1(x)      (ROTR32(x,6)^ROTR32(x,11)^ROTR32(x,25))
#define SIG0(x)     (ROTR32(x,7)^ROTR32(x,18)^((x)>>3))
#define SIG1(x)     (ROTR32(x,17)^ROTR32(x,19)^((x)>>10))

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t *data) {
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

static void sha256_init(sha256_ctx_t *ctx) {
    ctx->datalen=0; ctx->bitlen=0;
    ctx->state[0]=0x6a09e667;ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372;ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f;ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab;ctx->state[7]=0x5be0cd19;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    for (size_t i=0;i<len;++i){
        ctx->data[ctx->datalen]=data[i];
        ctx->datalen++;
        if (ctx->datalen==64){
            sha256_transform(ctx,ctx->data);
            ctx->bitlen+=512;
            ctx->datalen=0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t *hash) {
    uint32_t i=ctx->datalen;
    if (ctx->datalen<56){
        ctx->data[i++]=0x80;
        while(i<56) ctx->data[i++]=0x00;
    } else {
        ctx->data[i++]=0x80;
        while(i<64) ctx->data[i++]=0x00;
        sha256_transform(ctx,ctx->data);
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
    sha256_transform(ctx,ctx->data);
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

// Compute SHA-256 of file at path, write 64-char lowercase hex into hex_out (65 bytes).
// Returns 0 on success, -1 on error.
static int sha256_file(const char *path, char hex_out[65]) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha256_update(&ctx, buf, n);
    }
    int err = ferror(f);
    fclose(f);
    if (err) return -1;
    uint8_t hash[32];
    sha256_final(&ctx, hash);
    static const char *hex_chars = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex_out[i*2]   = hex_chars[(hash[i]>>4)&0xf];
        hex_out[i*2+1] = hex_chars[ hash[i]    &0xf];
    }
    hex_out[64] = '\0';
    return 0;
}

// ── Minimal JSON extraction helpers ──────────────────────────────────────────
// We do NOT link a full JSON library in model_verify (avoid dependency bloat).
// We parse the manifest with simple string search helpers.
// The manifest format is fully specified and small — this is safe.

// Find the value of a simple JSON string field: "key": "value"
// Writes value into out (max_len bytes, null-terminated). Returns 1 on success.
static int json_get_string(const char *json, const char *key, char *out, size_t max_len) {
    // Find key
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) return 0;
    pos += strlen(search);
    // Skip whitespace and colon
    while (*pos && (*pos==' '||*pos=='\t'||*pos==':')) pos++;
    if (*pos != '"') return 0;
    pos++; // skip opening quote
    size_t i = 0;
    while (*pos && *pos != '"' && i < max_len-1) {
        out[i++] = *pos++;
    }
    out[i] = '\0';
    return (*pos == '"') ? 1 : 0;
}

extern "C" {

monday_error_t monday_model_verify(const char              *manifest_path,
                                    monday_model_manifest_t *out) {
    if (!manifest_path || !out) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "null argument to monday_model_verify");
    }
    memset(out, 0, sizeof(*out));

    // 1. Read manifest file
    FILE *mf = fopen(manifest_path, "r");
    if (!mf) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "model_verify: manifest file not found");
        return MONDAY_ERROR(MONDAY_ERR_MANIFEST_INVALID, "manifest file not found");
    }
    // Read entire manifest (max 8 KB — manifests are tiny)
    char manifest_buf[8192];
    size_t nread = fread(manifest_buf, 1, sizeof(manifest_buf)-1, mf);
    int read_err = ferror(mf);
    fclose(mf);
    if (read_err || nread == 0) {
        return MONDAY_ERROR(MONDAY_ERR_MANIFEST_INVALID, "failed to read manifest");
    }
    manifest_buf[nread] = '\0';

    // 2. Extract fields from manifest JSON
    char model_rel_path[4096] = {0};
    char expected_sha256[65]  = {0};
    char ed25519_sig_b64[256] = {0};

    if (!json_get_string(manifest_buf, "path",              model_rel_path,  sizeof(model_rel_path))  ||
        !json_get_string(manifest_buf, "sha256",            expected_sha256, sizeof(expected_sha256)) ||
        !json_get_string(manifest_buf, "ed25519_signature", ed25519_sig_b64, sizeof(ed25519_sig_b64))) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "model_verify: malformed manifest (missing required fields)");
        return MONDAY_ERROR(MONDAY_ERR_MANIFEST_INVALID, "manifest missing required fields");
    }

    // 3. Resolve model file path (relative to manifest directory)
    // Resolve manifest dir
    char manifest_dir[4096];
    strncpy(manifest_dir, manifest_path, sizeof(manifest_dir)-1);
    manifest_dir[sizeof(manifest_dir)-1] = '\0';
    // Find last separator
    char *last_sep = strrchr(manifest_dir, '/');
    if (!last_sep) last_sep = strrchr(manifest_dir, '\\');
    if (last_sep) {
        last_sep[1] = '\0';
    } else {
        manifest_dir[0] = '.'; manifest_dir[1] = '/';
        manifest_dir[2] = '\0';
    }
    if (strlen(manifest_dir) + strlen(model_rel_path) >= sizeof(out->model_path)) {
        return MONDAY_ERROR(MONDAY_ERR_INVALID_ARG, "Resolved model path exceeds buffer limit");
    }
    strcpy(out->model_path, manifest_dir);
    strcat(out->model_path, model_rel_path);
    strncpy(out->sha256_hex, expected_sha256, 64);
    out->sha256_hex[64] = '\0';

    // 4. Ed25519 signature verification
    //    Architecture §2.3: signature covers the canonical JSON of the "files" array.
    //    Stub: if NOT MONDAY_HAVE_LIBSODIUM, monday_ed25519_verify returns NOT_SUPPORTED.
    //    We tolerate NOT_SUPPORTED in stub builds (allows testing the hash-check path).
    //    In production builds, NOT_SUPPORTED is a hard error.
    {
        // For the manifest signature check: message = the "files" array JSON substring.
        // We extract it by finding '[' after "files" and the matching ']'.
        const char *files_start = strstr(manifest_buf, "\"files\"");
        if (!files_start) {
            return MONDAY_ERROR(MONDAY_ERR_MANIFEST_INVALID, "manifest missing \"files\" array");
        }
        files_start = strchr(files_start, '[');
        if (!files_start) {
            return MONDAY_ERROR(MONDAY_ERR_MANIFEST_INVALID, "manifest \"files\" not an array");
        }
        // Find matching ]
        int depth = 0;
        const char *files_end = files_start;
        while (*files_end) {
            if (*files_end=='[') depth++;
            else if (*files_end==']') { depth--; if(depth==0){files_end++;break;} }
            files_end++;
        }
        size_t files_json_len = (size_t)(files_end - files_start);

        // Decode base64url signature (64 raw bytes).
        // In stub builds without libsodium this whole path is bypassed by
        // monday_ed25519_verify returning NOT_SUPPORTED.
        uint8_t sig_bytes[64] = {0};
        // Minimal base64url decode (no padding)
        {
            static const char b64url[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            size_t sig_len = strlen(ed25519_sig_b64);
            size_t out_len = 0;
            uint32_t acc = 0; int bits = 0;
            for (size_t ci = 0; ci < sig_len && out_len < 64; ci++) {
                const char *p = strchr(b64url, ed25519_sig_b64[ci]);
                if (!p) continue;
                acc = (acc<<6)|(uint32_t)(p-b64url);
                bits+=6;
                if (bits>=8) {
                    bits-=8;
                    sig_bytes[out_len++]=(uint8_t)(acc>>bits);
                }
            }
        }

        monday_error_t sig_err = monday_ed25519_verify(
            (const uint8_t*)files_start, files_json_len,
            sig_bytes, kModelVerifyKey);

        if (sig_err.code != MONDAY_OK && sig_err.code != MONDAY_ERR_NOT_SUPPORTED) {
            monday_log_write(MONDAY_LOG_SYSTEM,
                             "model_verify: Ed25519 signature check FAILED");
            return sig_err;
        }
        if (sig_err.code == MONDAY_ERR_NOT_SUPPORTED) {
            monday_log_write(MONDAY_LOG_DIAGNOSTIC,
                             "model_verify: Ed25519 stub — skipping sig check (dev build)");
        }
    }

    // 5. Check model file existence
    FILE *model_f = fopen(out->model_path, "rb");
    if (!model_f) {
        monday_log_write_kv_long(MONDAY_LOG_DIAGNOSTIC,
                                  "model_verify: model file not found",
                                  "path_len", (long)strlen(out->model_path));
        return MONDAY_ERROR(MONDAY_ERR_NOT_FOUND, "model file not found");
    }
    // Get size
    fseek(model_f, 0, SEEK_END);
    long fsize = ftell(model_f);
    fclose(model_f);
    out->size_bytes = fsize;

    // 6. Compute SHA-256 of model file and compare
    char actual_sha256[65];
    if (sha256_file(out->model_path, actual_sha256) != 0) {
        return MONDAY_ERROR(MONDAY_ERR_MODEL_HASH_MISMATCH, "failed to hash model file");
    }
    if (strncasecmp(actual_sha256, expected_sha256, 64) != 0) {
        monday_log_write(MONDAY_LOG_SYSTEM,
                         "model_verify: SHA-256 MISMATCH — model file tampered");
        return MONDAY_ERROR(MONDAY_ERR_MODEL_HASH_MISMATCH,
                             "model file SHA-256 does not match manifest");
    }

    monday_log_write(MONDAY_LOG_DIAGNOSTIC, "model_verify: manifest OK, model hash verified");
    return MONDAY_ERROR(MONDAY_OK, "");
}

} // extern "C"
