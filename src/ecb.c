#include "../include/ecb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <openssl/aes.h>

/**
 * Применение дополнения PKCS#7 к открытому тексту
 */
static unsigned char* apply_pkcs7_padding(const unsigned char* data, size_t data_len, size_t* padded_len) {
    // Вычисление необходимого дополнения
    size_t padding_bytes = AES_BLOCK_SIZE - (data_len % AES_BLOCK_SIZE);
    *padded_len = data_len + padding_bytes;

    unsigned char* padded = (unsigned char*)malloc(*padded_len);
    if (!padded) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        return NULL;
    }

    // Копирование исходных данных
    memcpy(padded, data, data_len);

    // Добавление байтов дополнения (каждый байт содержит длину дополнения)
    for (size_t i = 0; i < padding_bytes; i++) {
        padded[data_len + i] = (unsigned char)padding_bytes;
    }

    return padded;
}

/**
 * Удаление и проверка дополнения PKCS#7
 */
static unsigned char* remove_pkcs7_padding(const unsigned char* data, size_t data_len, size_t* unpadded_len) {
    if (data_len == 0 || data_len % AES_BLOCK_SIZE != 0) {
        fprintf(stderr, "Error: Invalid ciphertext length for decryption\n");
        return NULL;
    }

    // Получение длины дополнения из последнего байта
    unsigned char padding_len = data[data_len - 1];

    // Проверка длины дополнения
    if (padding_len == 0 || padding_len > AES_BLOCK_SIZE) {
        fprintf(stderr, "Error: Invalid PKCS#7 padding\n");
        return NULL;
    }

    // Проверка корректности всех байтов дополнения
    for (size_t i = data_len - padding_len; i < data_len; i++) {
        if (data[i] != padding_len) {
            fprintf(stderr, "Error: Invalid PKCS#7 padding\n");
            return NULL;
        }
    }

    *unpadded_len = data_len - padding_len;

    unsigned char* unpadded = (unsigned char*)malloc(*unpadded_len);
    if (!unpadded) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        return NULL;
    }

    memcpy(unpadded, data, *unpadded_len);
    return unpadded;
}

unsigned char* aes_ecb_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, size_t* output_size) {
    // Применение дополнения PKCS#7
    size_t padded_len;
    unsigned char* padded = apply_pkcs7_padding(plaintext, plaintext_len, &padded_len);
    if (!padded) {
        return NULL;
    }

    // Выделение буфера для выходных данных
    unsigned char* ciphertext = (unsigned char*)malloc(padded_len);
    if (!ciphertext) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        free(padded);
        return NULL;
    }

    // Настройка ключа AES
    AES_KEY aes_key;
    if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
        fprintf(stderr, "Error: Failed to set AES encryption key\n");
        free(padded);
        free(ciphertext);
        return NULL;
    }

    // Шифрование каждого блока
    for (size_t i = 0; i < padded_len; i += AES_BLOCK_SIZE) {
        AES_encrypt(padded + i, ciphertext + i, &aes_key);
    }

    free(padded);
    *output_size = padded_len;
    return ciphertext;
}

unsigned char* aes_ecb_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, size_t* output_size) {
    if (ciphertext_len % AES_BLOCK_SIZE != 0) {
        fprintf(stderr, "Error: Ciphertext length must be a multiple of %d bytes\n", AES_BLOCK_SIZE);
        return NULL;
    }

    // Выделение буфера для дешифрованных данных (с дополнением)
    unsigned char* decrypted_padded = (unsigned char*)malloc(ciphertext_len);
    if (!decrypted_padded) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        return NULL;
    }

    // Настройка ключа AES
    AES_KEY aes_key;
    if (AES_set_decrypt_key(key, 128, &aes_key) < 0) {
        fprintf(stderr, "Error: Failed to set AES decryption key\n");
        free(decrypted_padded);
        return NULL;
    }

    // Дешифрование каждого блока
    for (size_t i = 0; i < ciphertext_len; i += AES_BLOCK_SIZE) {
        AES_decrypt(ciphertext + i, decrypted_padded + i, &aes_key);
    }

    // Удаление дополнения PKCS#7
    unsigned char* plaintext = remove_pkcs7_padding(decrypted_padded, ciphertext_len, output_size);
    free(decrypted_padded);

    return plaintext;
}

unsigned char* hex_to_bytes(const char* hex_string, size_t* size) {
    size_t hex_len = strlen(hex_string);

    // Проверка длины hex-строки
    if (hex_len % 2 != 0) {
        fprintf(stderr, "Error: Hex string must have an even length\n");
        return NULL;
    }

    *size = hex_len / 2;
    unsigned char* bytes = (unsigned char*)malloc(*size);
    if (!bytes) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        return NULL;
    }

    // Преобразование каждой пары hex-символов в байт
    for (size_t i = 0; i < *size; i++) {
        char hex_byte[3] = {hex_string[i * 2], hex_string[i * 2 + 1], '\0'};

        // Проверка hex-символов
        if (!isxdigit(hex_byte[0]) || !isxdigit(hex_byte[1])) {
            fprintf(stderr, "Error: Invalid hex character in key\n");
            free(bytes);
            return NULL;
        }

        bytes[i] = (unsigned char)strtol(hex_byte, NULL, 16);
    }

    return bytes;
}
