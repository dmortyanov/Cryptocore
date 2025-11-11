#ifndef HASH_H
#define HASH_H

#include <stddef.h>
#include <stdint.h>

// SHA-256 context structure
typedef struct {
    uint32_t state[8];           // Hash state (A, B, C, D, E, F, G, H)
    uint64_t bit_count;          // Total bits processed
    uint8_t buffer[64];          // Current block buffer (512 bits)
    size_t buffer_len;           // Bytes in buffer
} sha256_ctx_t;

// SHA-256 functions
void sha256_init(sha256_ctx_t* ctx);
void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len);
void sha256_final(sha256_ctx_t* ctx, uint8_t* hash);
int sha256_hash_file(const char* filepath, uint8_t* hash);  // Returns 0 on success, -1 on error

// SHA3-256 using OpenSSL (simpler implementation)
int sha3_256_hash_file(const char* filepath, uint8_t* hash);  // Returns 0 on success, -1 on error

// Utility: convert hash to hex string
void hash_to_hex(const uint8_t* hash, size_t hash_len, char* hex_out);

#endif // HASH_H

