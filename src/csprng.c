#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rand.h>
#include "include/csprng.h"

/**
 * Генерация криптографически стойких случайных байтов
 */
int generate_random_bytes(unsigned char *buffer, size_t num_bytes) {
    if (!buffer || num_bytes == 0) {
        return -1;
    }
    
    // Используем OpenSSL RAND_bytes для криптографически стойкой генерации
    if (RAND_bytes(buffer, (int)num_bytes) != 1) {
        fprintf(stderr, "Error: Failed to generate cryptographically secure random bytes\n");
        return -1;
    }
    
    return 0;
}

/**
 * Генерация случайного ключа для AES-128
 */
int generate_aes_key(unsigned char *key) {
    return generate_random_bytes(key, 16);
}

/**
 * Генерация случайного IV для режимов с IV
 */
int generate_random_iv(unsigned char *iv) {
    return generate_random_bytes(iv, 16);
}

/**
 * Проверка слабых ключей
 */
int is_weak_key(const unsigned char *key, size_t key_len) {
    if (!key || key_len == 0) {
        return 0;
    }
    
    // Проверка на ключ из нулей
    int all_zeros = 1;
    for (size_t i = 0; i < key_len; i++) {
        if (key[i] != 0) {
            all_zeros = 0;
            break;
        }
    }
    if (all_zeros) {
        return 1;
    }
    
    // Проверка на последовательные байты
    int sequential = 1;
    for (size_t i = 1; i < key_len; i++) {
        if (key[i] != key[i-1] + 1) {
            sequential = 0;
            break;
        }
    }
    if (sequential) {
        return 1;
    }
    
    // Проверка на обратную последовательность
    int reverse_sequential = 1;
    for (size_t i = 1; i < key_len; i++) {
        if (key[i] != key[i-1] - 1) {
            reverse_sequential = 0;
            break;
        }
    }
    if (reverse_sequential) {
        return 1;
    }
    
    // Проверка на повторяющийся паттерн
    if (key_len >= 2) {
        int repeating_pattern = 1;
        for (size_t i = 2; i < key_len; i++) {
            if (key[i] != key[i % 2]) {
                repeating_pattern = 0;
                break;
            }
        }
        if (repeating_pattern) {
            return 1;
        }
    }
    
    return 0;
}
