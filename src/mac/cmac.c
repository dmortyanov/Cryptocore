#include "../../include/mac.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/aes.h>

#define AES_BLOCK_SIZE 16
#define RB 0x87  // Constant for 128-bit blocks (NIST SP 800-38B)

/**
 * Left shift by 1 bit (for subkey generation)
 */
static void left_shift_one(const uint8_t* input, uint8_t* output) {
    uint8_t carry = 0;
    for (int i = AES_BLOCK_SIZE - 1; i >= 0; i--) {
        output[i] = (input[i] << 1) | carry;
        carry = (input[i] >> 7) & 1;
    }
}

/**
 * Generate subkeys K1 and K2 according to NIST SP 800-38B
 */
static int generate_subkeys(const uint8_t* key, uint8_t* k1, uint8_t* k2) {
    AES_KEY aes_key;
    uint8_t L[AES_BLOCK_SIZE];
    uint8_t temp[AES_BLOCK_SIZE];
    
    // Set up AES key
    if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
        return -1;
    }
    
    // L = AES-Encrypt(0^128, K)
    memset(L, 0, AES_BLOCK_SIZE);
    AES_encrypt(L, L, &aes_key);
    
    // Generate K1
    left_shift_one(L, k1);
    if (L[0] & 0x80) {  // If MSB(L) = 1
        k1[AES_BLOCK_SIZE - 1] ^= RB;
    }
    
    // Generate K2
    left_shift_one(k1, k2);
    if (k1[0] & 0x80) {  // If MSB(K1) = 1
        k2[AES_BLOCK_SIZE - 1] ^= RB;
    }
    
    return 0;
}

/**
 * XOR two blocks
 */
static void xor_blocks(uint8_t* out, const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < AES_BLOCK_SIZE; i++) {
        out[i] = a[i] ^ b[i];
    }
}

/**
 * Initialize AES-CMAC context with a key
 */
int cmac_init(cmac_ctx_t* ctx, const uint8_t* key) {
    if (!ctx || !key) {
        return -1;
    }
    
    // Generate subkeys
    if (generate_subkeys(key, ctx->k1, ctx->k2) != 0) {
        return -1;
    }
    
    // Store key
    memcpy(ctx->key, key, AES_BLOCK_SIZE);
    
    // Initialize state to zero
    memset(ctx->prev_encrypted, 0, AES_BLOCK_SIZE);
    memset(ctx->last_block, 0, AES_BLOCK_SIZE);
    memset(ctx->partial_block, 0, AES_BLOCK_SIZE);
    ctx->block_offset = 0;
    ctx->has_last_block = 0;
    ctx->initialized = 1;
    
    return 0;
}

/**
 * Update CMAC with new data (streaming)
 */
int cmac_update(cmac_ctx_t* ctx, const uint8_t* data, size_t len) {
    if (!ctx || !ctx->initialized || !data) {
        return -1;
    }
    
    AES_KEY aes_key;
    if (AES_set_encrypt_key(ctx->key, 128, &aes_key) < 0) {
        return -1;
    }
    
    size_t i = 0;
    
    // Process any partial block first
    if (ctx->block_offset > 0) {
        size_t to_copy = AES_BLOCK_SIZE - ctx->block_offset;
        if (to_copy > len) {
            to_copy = len;
        }
        
        memcpy(ctx->partial_block + ctx->block_offset, data, to_copy);
        ctx->block_offset += to_copy;
        i += to_copy;
        
        // If block is complete, encrypt it (CBC-MAC)
        if (ctx->block_offset == AES_BLOCK_SIZE) {
            uint8_t encrypted[AES_BLOCK_SIZE];
            uint8_t block[AES_BLOCK_SIZE];
            // CBC-MAC: XOR with previous encrypted state
            memcpy(block, ctx->partial_block, AES_BLOCK_SIZE);
            xor_blocks(block, block, ctx->prev_encrypted);
            AES_encrypt(block, encrypted, &aes_key);
            memcpy(ctx->prev_encrypted, encrypted, AES_BLOCK_SIZE);
            ctx->block_offset = 0;
        }
    }
    
    // Process full blocks (but don't encrypt the last one if it's the final block)
    while (i + AES_BLOCK_SIZE <= len) {
        // If we have a pending last block, encrypt it first
        if (ctx->has_last_block) {
            uint8_t encrypted[AES_BLOCK_SIZE];
            uint8_t block[AES_BLOCK_SIZE];
            // CBC-MAC: XOR with previous encrypted state
            memcpy(block, ctx->last_block, AES_BLOCK_SIZE);
            xor_blocks(block, block, ctx->prev_encrypted);
            AES_encrypt(block, encrypted, &aes_key);
            memcpy(ctx->prev_encrypted, encrypted, AES_BLOCK_SIZE);
            ctx->has_last_block = 0;
        }
        
        // Store current block as potential last block
        memcpy(ctx->last_block, data + i, AES_BLOCK_SIZE);
        ctx->has_last_block = 1;
        
        i += AES_BLOCK_SIZE;
    }
    
    // Store remaining bytes in partial_block (incomplete block)
    if (i < len) {
        size_t remaining = len - i;
        // Clear any previous incomplete block data
        if (ctx->block_offset == 0) {
            memset(ctx->partial_block, 0, AES_BLOCK_SIZE);
        }
        memcpy(ctx->partial_block + ctx->block_offset, data + i, remaining);
        ctx->block_offset += remaining;
    }
    
    return 0;
}

/**
 * Finalize CMAC computation
 */
int cmac_final(cmac_ctx_t* ctx, uint8_t* mac) {
    if (!ctx || !ctx->initialized || !mac) {
        return -1;
    }
    
    AES_KEY aes_key;
    if (AES_set_encrypt_key(ctx->key, 128, &aes_key) < 0) {
        return -1;
    }
    
    uint8_t final_block[AES_BLOCK_SIZE];
    
    if (ctx->has_last_block) {
        // Last block is complete, XOR with previous encrypted state, then K1
        xor_blocks(final_block, ctx->last_block, ctx->prev_encrypted);
        xor_blocks(final_block, final_block, ctx->k1);
    } else if (ctx->block_offset > 0) {
        // Last block is incomplete, pad and use K2
        // Padding: add 0x80, then zeros
        memcpy(final_block, ctx->partial_block, ctx->block_offset);
        final_block[ctx->block_offset] = 0x80;
        if (ctx->block_offset + 1 < AES_BLOCK_SIZE) {
            memset(final_block + ctx->block_offset + 1, 0, AES_BLOCK_SIZE - ctx->block_offset - 1);
        }
        
        // XOR with previous encrypted state, then K2
        xor_blocks(final_block, final_block, ctx->prev_encrypted);
        xor_blocks(final_block, final_block, ctx->k2);
    } else {
        // Empty message - use K2
        memset(final_block, 0, AES_BLOCK_SIZE);
        final_block[0] = 0x80;  // Padding
        xor_blocks(final_block, final_block, ctx->k2);
    }
    
    // Final encryption
    AES_encrypt(final_block, mac, &aes_key);
    
    ctx->initialized = 0;
    return 0;
}

/**
 * Compute AES-CMAC for a file (convenience function with chunked processing)
 */
int cmac_file(const char* filepath, const uint8_t* key, uint8_t* mac) {
    cmac_ctx_t ctx;
    FILE* f;
    uint8_t buffer[4096];
    size_t bytes_read;
    
    if (!filepath || !key || !mac) {
        return -1;
    }
    
    f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Failed to open file '%s'\n", filepath);
        return -1;
    }
    
    if (cmac_init(&ctx, key) != 0) {
        fclose(f);
        return -1;
    }
    
    // Process file in chunks
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (cmac_update(&ctx, buffer, bytes_read) != 0) {
            fclose(f);
            return -1;
        }
    }
    
    if (ferror(f)) {
        fprintf(stderr, "Error: Failed to read file '%s'\n", filepath);
        fclose(f);
        return -1;
    }
    
    fclose(f);
    
    if (cmac_final(&ctx, mac) != 0) {
        return -1;
    }
    
    return 0;
}

