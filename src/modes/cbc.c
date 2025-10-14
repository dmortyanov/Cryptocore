#include "../../include/modes.h"
#include "../../include/ecb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        fprintf(stderr, "Ошибка: Не удалось выделить память\n");
        return NULL;
    }

    // Копирование исходных данных
    memcpy(padded, data, data_len);

    // Добавление байтов дополнения
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
        fprintf(stderr, "Ошибка: Неверная длина шифртекста для дешифрования\n");
        return NULL;
    }

    // Получение длины дополнения из последнего байта
    unsigned char padding_len = data[data_len - 1];

    // Проверка длины дополнения
    if (padding_len == 0 || padding_len > AES_BLOCK_SIZE) {
        fprintf(stderr, "Ошибка: Неверное дополнение PKCS#7\n");
        return NULL;
    }

    // Проверка корректности всех байтов дополнения
    for (size_t i = data_len - padding_len; i < data_len; i++) {
        if (data[i] != padding_len) {
            fprintf(stderr, "Ошибка: Неверное дополнение PKCS#7\n");
            return NULL;
        }
    }

    *unpadded_len = data_len - padding_len;

    unsigned char* unpadded = (unsigned char*)malloc(*unpadded_len);
    if (!unpadded) {
        fprintf(stderr, "Ошибка: Не удалось выделить память\n");
        return NULL;
    }

    memcpy(unpadded, data, *unpadded_len);
    return unpadded;
}

unsigned char* aes_cbc_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size) {
    // Применение дополнения PKCS#7
    size_t padded_len;
    unsigned char* padded = apply_pkcs7_padding(plaintext, plaintext_len, &padded_len);
    if (!padded) {
        return NULL;
    }

    // Выделение буфера для выходных данных
    unsigned char* ciphertext = (unsigned char*)malloc(padded_len);
    if (!ciphertext) {
        fprintf(stderr, "Ошибка: Не удалось выделить память\n");
        free(padded);
        return NULL;
    }

    // Настройка ключа AES
    AES_KEY aes_key;
    if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
        fprintf(stderr, "Ошибка: Не удалось установить ключ шифрования AES\n");
        free(padded);
        free(ciphertext);
        return NULL;
    }

    // Буфер для предыдущего блока шифртекста (начинается с IV)
    unsigned char prev_block[AES_BLOCK_SIZE];
    memcpy(prev_block, iv, AES_BLOCK_SIZE);

    // Шифрование каждого блока в режиме CBC
    for (size_t i = 0; i < padded_len; i += AES_BLOCK_SIZE) {
        unsigned char xored_block[AES_BLOCK_SIZE];
        
        // XOR текущего блока открытого текста с предыдущим блоком шифртекста
        xor_blocks(xored_block, padded + i, prev_block, AES_BLOCK_SIZE);
        
        // Шифрование результата XOR
        AES_encrypt(xored_block, ciphertext + i, &aes_key);
        
        // Сохранение текущего блока шифртекста для следующей итерации
        memcpy(prev_block, ciphertext + i, AES_BLOCK_SIZE);
    }

    free(padded);
    *output_size = padded_len;
    return ciphertext;
}

unsigned char* aes_cbc_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size) {
    if (ciphertext_len % AES_BLOCK_SIZE != 0) {
        fprintf(stderr, "Ошибка: Длина шифртекста должна быть кратна %d байтам\n", AES_BLOCK_SIZE);
        return NULL;
    }

    // Выделение буфера для дешифрованных данных (с дополнением)
    unsigned char* decrypted_padded = (unsigned char*)malloc(ciphertext_len);
    if (!decrypted_padded) {
        fprintf(stderr, "Ошибка: Не удалось выделить память\n");
        return NULL;
    }

    // Настройка ключа AES
    AES_KEY aes_key;
    if (AES_set_decrypt_key(key, 128, &aes_key) < 0) {
        fprintf(stderr, "Ошибка: Не удалось установить ключ дешифрования AES\n");
        free(decrypted_padded);
        return NULL;
    }

    // Буфер для предыдущего блока шифртекста (начинается с IV)
    unsigned char prev_block[AES_BLOCK_SIZE];
    memcpy(prev_block, iv, AES_BLOCK_SIZE);

    // Дешифрование каждого блока в режиме CBC
    for (size_t i = 0; i < ciphertext_len; i += AES_BLOCK_SIZE) {
        unsigned char decrypted_block[AES_BLOCK_SIZE];
        
        // Дешифрование текущего блока
        AES_decrypt(ciphertext + i, decrypted_block, &aes_key);
        
        // XOR с предыдущим блоком шифртекста
        xor_blocks(decrypted_padded + i, decrypted_block, prev_block, AES_BLOCK_SIZE);
        
        // Сохранение текущего блока шифртекста для следующей итерации
        memcpy(prev_block, ciphertext + i, AES_BLOCK_SIZE);
    }

    // Удаление дополнения PKCS#7
    unsigned char* plaintext = remove_pkcs7_padding(decrypted_padded, ciphertext_len, output_size);
    free(decrypted_padded);

    return plaintext;
}

