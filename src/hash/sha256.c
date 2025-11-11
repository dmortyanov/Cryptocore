#include "../../include/hash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// SHA-256 constants (first 32 bits of fractional parts of cube roots of first 64 primes)
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Helper macros
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n) ((x) >> (n))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define BSIG1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SSIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define SSIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))

// Initialize SHA-256 context
void sha256_init(sha256_ctx_t* ctx) {
    // First 32 bits of fractional parts of square roots of first 8 primes
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    
    ctx->bit_count = 0;
    ctx->buffer_len = 0;
}

// Process one 512-bit block
static void sha256_transform(sha256_ctx_t* ctx, const uint8_t* block) {
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t T1, T2;
    int i;
    
    // Prepare message schedule (W[0..63])
    for (i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    
    for (i = 16; i < 64; i++) {
        W[i] = SSIG1(W[i - 2]) + W[i - 7] + SSIG0(W[i - 15]) + W[i - 16];
    }
    
    // Initialize working variables
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    
    // Main loop (64 rounds)
    for (i = 0; i < 64; i++) {
        T1 = h + BSIG1(e) + CH(e, f, g) + K[i] + W[i];
        T2 = BSIG0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }
    
    // Update hash state
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

// Update hash with new data
void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len) {
    size_t i;
    
    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buffer_len++] = data[i];
        
        // Process block when buffer is full
        if (ctx->buffer_len == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bit_count += 512;
            ctx->buffer_len = 0;
        }
    }
}

// Finalize hash computation
void sha256_final(sha256_ctx_t* ctx, uint8_t* hash) {
    size_t i;
    uint64_t total_bits;
    
    // Calculate total bits
    total_bits = ctx->bit_count + (ctx->buffer_len * 8);
    
    // Append padding bit '1'
    ctx->buffer[ctx->buffer_len++] = 0x80;
    
    // Append padding '0' bits
    if (ctx->buffer_len > 56) {
        // Not enough space for length, need extra block
        while (ctx->buffer_len < 64) {
            ctx->buffer[ctx->buffer_len++] = 0x00;
        }
        sha256_transform(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    
    // Pad with zeros until we have 56 bytes
    while (ctx->buffer_len < 56) {
        ctx->buffer[ctx->buffer_len++] = 0x00;
    }
    
    // Append message length (64 bits, big-endian)
    for (i = 0; i < 8; i++) {
        ctx->buffer[56 + i] = (uint8_t)(total_bits >> (56 - i * 8));
    }
    
    // Process final block
    sha256_transform(ctx, ctx->buffer);
    
    // Convert hash state to output bytes (big-endian)
    for (i = 0; i < 8; i++) {
        hash[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

// Hash a file using SHA-256 (chunked processing)
// Returns 0 on success, -1 on error
int sha256_hash_file(const char* filepath, uint8_t* hash) {
    sha256_ctx_t ctx;
    FILE* f = fopen(filepath, "rb");
    uint8_t buffer[4096];
    size_t bytes_read;
    
    if (!f) {
        fprintf(stderr, "Error: Failed to open file '%s'\n", filepath);
        return -1;
    }
    
    sha256_init(&ctx);
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        sha256_update(&ctx, buffer, bytes_read);
    }
    
    fclose(f);
    sha256_final(&ctx, hash);
    return 0;
}

// Convert hash bytes to hex string
void hash_to_hex(const uint8_t* hash, size_t hash_len, char* hex_out) {
    const char hex_chars[] = "0123456789abcdef";
    size_t i;
    
    for (i = 0; i < hash_len; i++) {
        hex_out[i * 2] = hex_chars[hash[i] >> 4];
        hex_out[i * 2 + 1] = hex_chars[hash[i] & 0x0f];
    }
    hex_out[hash_len * 2] = '\0';
}

