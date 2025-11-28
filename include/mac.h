#ifndef MAC_H
#define MAC_H

#include "hash.h"
#include <stddef.h>
#include <stdint.h>

/**
 * HMAC context structure
 */
typedef struct {
    uint8_t key[64];              // Processed key (always 64 bytes)
    sha256_ctx_t inner_ctx;       // Inner hash context
    sha256_ctx_t outer_ctx;       // Outer hash context
    int initialized;               // Initialization flag
} hmac_ctx_t;

/**
 * Initialize HMAC context with a key
 * 
 * @param ctx HMAC context to initialize
 * @param key Key bytes (can be any length)
 * @param key_len Length of key in bytes
 * @return 0 on success, -1 on error
 */
int hmac_init(hmac_ctx_t* ctx, const uint8_t* key, size_t key_len);

/**
 * Update HMAC with new data (streaming)
 * 
 * @param ctx HMAC context
 * @param data Data to process
 * @param len Length of data in bytes
 */
void hmac_update(hmac_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * Finalize HMAC computation
 * 
 * @param ctx HMAC context
 * @param mac Output buffer for MAC (32 bytes for SHA-256)
 */
void hmac_final(hmac_ctx_t* ctx, uint8_t* mac);

/**
 * Compute HMAC for a file (convenience function)
 * 
 * @param filepath Path to input file
 * @param key Key bytes
 * @param key_len Length of key in bytes
 * @param mac Output buffer for MAC (32 bytes)
 * @return 0 on success, -1 on error
 */
int hmac_file(const char* filepath, const uint8_t* key, size_t key_len, uint8_t* mac);

#endif // MAC_H

