#include "../../include/mac.h"
#include "../../include/hash.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define HMAC_BLOCK_SIZE 64  // Block size for SHA-256

/**
 * Process key according to RFC 2104:
 * - If key_len > block_size: hash the key
 * - If key_len < block_size: pad with zeros
 * - Result is always block_size bytes
 */
static void process_key(uint8_t* processed_key, const uint8_t* key, size_t key_len) {
    if (key_len > HMAC_BLOCK_SIZE) {
        // Key is longer than block size: hash it
        sha256_ctx_t ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, key, key_len);
        sha256_final(&ctx, processed_key);
        // Pad remaining bytes with zeros
        memset(processed_key + 32, 0, HMAC_BLOCK_SIZE - 32);
    } else {
        // Key is shorter or equal: copy and pad with zeros
        memcpy(processed_key, key, key_len);
        if (key_len < HMAC_BLOCK_SIZE) {
            memset(processed_key + key_len, 0, HMAC_BLOCK_SIZE - key_len);
        }
    }
}

/**
 * XOR two byte arrays
 */
static void xor_bytes(uint8_t* out, const uint8_t* a, const uint8_t* b, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        out[i] = a[i] ^ b[i];
    }
}

/**
 * Initialize HMAC context with a key
 */
int hmac_init(hmac_ctx_t* ctx, const uint8_t* key, size_t key_len) {
    if (!ctx || !key || key_len == 0) {
        return -1;
    }
    
    // Process the key
    process_key(ctx->key, key, key_len);
    
    // Create inner pad: key XOR ipad (0x36 repeated)
    uint8_t ipad[HMAC_BLOCK_SIZE];
    memset(ipad, 0x36, HMAC_BLOCK_SIZE);
    uint8_t inner_key[HMAC_BLOCK_SIZE];
    xor_bytes(inner_key, ctx->key, ipad, HMAC_BLOCK_SIZE);
    
    // Initialize inner hash context with (K ⊕ ipad)
    sha256_init(&ctx->inner_ctx);
    sha256_update(&ctx->inner_ctx, inner_key, HMAC_BLOCK_SIZE);
    
    // Create outer pad: key XOR opad (0x5c repeated)
    uint8_t opad[HMAC_BLOCK_SIZE];
    memset(opad, 0x5c, HMAC_BLOCK_SIZE);
    uint8_t outer_key[HMAC_BLOCK_SIZE];
    xor_bytes(outer_key, ctx->key, opad, HMAC_BLOCK_SIZE);
    
    // Initialize outer hash context (will be used in final)
    sha256_init(&ctx->outer_ctx);
    sha256_update(&ctx->outer_ctx, outer_key, HMAC_BLOCK_SIZE);
    
    ctx->initialized = 1;
    return 0;
}

/**
 * Update HMAC with new data (streaming)
 */
void hmac_update(hmac_ctx_t* ctx, const uint8_t* data, size_t len) {
    if (!ctx || !ctx->initialized || !data || len == 0) {
        return;
    }
    
    // Update inner hash with message data
    sha256_update(&ctx->inner_ctx, data, len);
}

/**
 * Finalize HMAC computation
 * HMAC(K, m) = H((K ⊕ opad) ∥ H((K ⊕ ipad) ∥ m))
 */
void hmac_final(hmac_ctx_t* ctx, uint8_t* mac) {
    if (!ctx || !ctx->initialized || !mac) {
        return;
    }
    
    // Finalize inner hash: H((K ⊕ ipad) ∥ m)
    uint8_t inner_hash[32];
    sha256_final(&ctx->inner_ctx, inner_hash);
    
    // Compute outer hash: H((K ⊕ opad) ∥ inner_hash)
    // Note: outer_ctx already has (K ⊕ opad) in it, we just need to add inner_hash
    sha256_update(&ctx->outer_ctx, inner_hash, 32);
    sha256_final(&ctx->outer_ctx, mac);
    
    ctx->initialized = 0;
}

/**
 * Compute HMAC for a file (convenience function with chunked processing)
 */
int hmac_file(const char* filepath, const uint8_t* key, size_t key_len, uint8_t* mac) {
    hmac_ctx_t ctx;
    FILE* f;
    uint8_t buffer[4096];
    size_t bytes_read;
    
    if (!filepath || !key || key_len == 0 || !mac) {
        return -1;
    }
    
    f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Failed to open file '%s'\n", filepath);
        return -1;
    }
    
    if (hmac_init(&ctx, key, key_len) != 0) {
        fclose(f);
        return -1;
    }
    
    // Process file in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        hmac_update(&ctx, buffer, bytes_read);
    }
    
    if (ferror(f)) {
        fprintf(stderr, "Error: Failed to read file '%s'\n", filepath);
        fclose(f);
        return -1;
    }
    
    fclose(f);
    hmac_final(&ctx, mac);
    return 0;
}

