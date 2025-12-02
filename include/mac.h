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

/**
 * AES-CMAC context structure
 */
typedef struct {
    uint8_t key[16];              // AES-128 key (16 bytes)
    uint8_t k1[16];               // First subkey
    uint8_t k2[16];               // Second subkey
    uint8_t prev_encrypted[16];  // Previous encrypted CBC-MAC result
    uint8_t last_block[16];      // Last complete block (not yet encrypted)
    uint8_t partial_block[16];   // Partial block (incomplete)
    size_t block_offset;          // Offset in partial block
    int has_last_block;           // Flag: last block is complete but not encrypted
    int initialized;              // Initialization flag
} cmac_ctx_t;

/**
 * Initialize AES-CMAC context with a key
 * 
 * @param ctx CMAC context to initialize
 * @param key Key bytes (must be 16 bytes for AES-128)
 * @return 0 on success, -1 on error
 */
int cmac_init(cmac_ctx_t* ctx, const uint8_t* key);

/**
 * Update CMAC with new data (streaming)
 * 
 * @param ctx CMAC context
 * @param data Data to process
 * @param len Length of data in bytes
 * @return 0 on success, -1 on error
 */
int cmac_update(cmac_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * Finalize CMAC computation
 * 
 * @param ctx CMAC context
 * @param mac Output buffer for MAC (16 bytes for AES-128)
 * @return 0 on success, -1 on error
 */
int cmac_final(cmac_ctx_t* ctx, uint8_t* mac);

/**
 * Compute AES-CMAC for a file (convenience function)
 * 
 * @param filepath Path to input file
 * @param key Key bytes (16 bytes for AES-128)
 * @param mac Output buffer for MAC (16 bytes)
 * @return 0 on success, -1 on error
 */
int cmac_file(const char* filepath, const uint8_t* key, uint8_t* mac);

#endif // MAC_H

