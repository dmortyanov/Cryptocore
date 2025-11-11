#include "../../include/modes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rand.h>

void xor_blocks(unsigned char* output, const unsigned char* a, const unsigned char* b, size_t len) {
    for (size_t i = 0; i < len; i++) {
        output[i] = a[i] ^ b[i];
    }
}

unsigned char* generate_iv(void) {
    unsigned char* iv = (unsigned char*)malloc(AES_BLOCK_SIZE);
    if (!iv) {
        fprintf(stderr, "Error: Failed to allocate memory for IV\n");
        return NULL;
    }

    // Генерация криптографически стойкого случайного IV с использованием OpenSSL
    if (RAND_bytes(iv, AES_BLOCK_SIZE) != 1) {
        fprintf(stderr, "Error: Failed to generate random IV\n");
        free(iv);
        return NULL;
    }

    return iv;
}

