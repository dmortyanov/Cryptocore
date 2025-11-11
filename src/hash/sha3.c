#include "../../include/hash.h"
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>

// Hash a file using SHA3-256 (via OpenSSL)
// Returns 0 on success, -1 on error
int sha3_256_hash_file(const char* filepath, uint8_t* hash) {
    EVP_MD_CTX* ctx = NULL;
    const EVP_MD* md = NULL;
    FILE* f = NULL;
    uint8_t buffer[4096];
    size_t bytes_read;
    unsigned int hash_len = 32;
    
    f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Failed to open file '%s'\n", filepath);
        return -1;
    }
    
    // Get SHA3-256 algorithm
    md = EVP_sha3_256();
    if (!md) {
        fprintf(stderr, "Error: SHA3-256 not available\n");
        fclose(f);
        return -1;
    }
    
    // Create and initialize context
    ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create hash context\n");
        fclose(f);
        return -1;
    }
    
    if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
        fprintf(stderr, "Error: Failed to initialize digest\n");
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return -1;
    }
    
    // Read file in chunks and update hash
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (EVP_DigestUpdate(ctx, buffer, bytes_read) != 1) {
            fprintf(stderr, "Error: Failed to update digest\n");
            EVP_MD_CTX_free(ctx);
            fclose(f);
            return -1;
        }
    }
    
    // Finalize hash
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        fprintf(stderr, "Error: Failed to finalize digest\n");
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return -1;
    }
    
    EVP_MD_CTX_free(ctx);
    fclose(f);
    return 0;
}

