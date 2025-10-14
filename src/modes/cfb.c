#include "../../include/modes.h"
#include "../../include/ecb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>

unsigned char* aes_cfb_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size) {
    // CFB - потоковый шифр, дополнение не требуется
    unsigned char* ciphertext = (unsigned char*)malloc(plaintext_len);
    if (!ciphertext) {
        fprintf(stderr, "Ошибка: Не удалось выделить память\n");
        return NULL;
    }

    // Настройка ключа AES для шифрования
    AES_KEY aes_key;
    if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
        fprintf(stderr, "Ошибка: Не удалось установить ключ шифрования AES\n");
        free(ciphertext);
        return NULL;
    }

    // Регистр сдвига (начинается с IV)
    unsigned char shift_register[AES_BLOCK_SIZE];
    memcpy(shift_register, iv, AES_BLOCK_SIZE);

    size_t i = 0;
    // Обработка полных блоков
    while (i + AES_BLOCK_SIZE <= plaintext_len) {
        unsigned char encrypted_register[AES_BLOCK_SIZE];
        
        // Шифрование регистра сдвига
        AES_encrypt(shift_register, encrypted_register, &aes_key);
        
        // XOR с блоком открытого текста
        xor_blocks(ciphertext + i, plaintext + i, encrypted_register, AES_BLOCK_SIZE);
        
        // Обновление регистра сдвига (становится блоком шифртекста)
        memcpy(shift_register, ciphertext + i, AES_BLOCK_SIZE);
        
        i += AES_BLOCK_SIZE;
    }

    // Обработка последнего неполного блока (если есть)
    if (i < plaintext_len) {
        unsigned char encrypted_register[AES_BLOCK_SIZE];
        size_t remaining = plaintext_len - i;
        
        // Шифрование регистра сдвига
        AES_encrypt(shift_register, encrypted_register, &aes_key);
        
        // XOR только с оставшимися байтами
        xor_blocks(ciphertext + i, plaintext + i, encrypted_register, remaining);
    }

    *output_size = plaintext_len;
    return ciphertext;
}

unsigned char* aes_cfb_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size) {
    // CFB дешифрование - то же самое, что и шифрование, но XOR с шифртекстом
    unsigned char* plaintext = (unsigned char*)malloc(ciphertext_len);
    if (!plaintext) {
        fprintf(stderr, "Ошибка: Не удалось выделить память\n");
        return NULL;
    }

    // Настройка ключа AES для шифрования (даже для дешифрования!)
    AES_KEY aes_key;
    if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
        fprintf(stderr, "Ошибка: Не удалось установить ключ шифрования AES\n");
        free(plaintext);
        return NULL;
    }

    // Регистр сдвига (начинается с IV)
    unsigned char shift_register[AES_BLOCK_SIZE];
    memcpy(shift_register, iv, AES_BLOCK_SIZE);

    size_t i = 0;
    // Обработка полных блоков
    while (i + AES_BLOCK_SIZE <= ciphertext_len) {
        unsigned char encrypted_register[AES_BLOCK_SIZE];
        
        // Шифрование регистра сдвига
        AES_encrypt(shift_register, encrypted_register, &aes_key);
        
        // XOR с блоком шифртекста
        xor_blocks(plaintext + i, ciphertext + i, encrypted_register, AES_BLOCK_SIZE);
        
        // Обновление регистра сдвига (становится блоком шифртекста)
        memcpy(shift_register, ciphertext + i, AES_BLOCK_SIZE);
        
        i += AES_BLOCK_SIZE;
    }

    // Обработка последнего неполного блока (если есть)
    if (i < ciphertext_len) {
        unsigned char encrypted_register[AES_BLOCK_SIZE];
        size_t remaining = ciphertext_len - i;
        
        // Шифрование регистра сдвига
        AES_encrypt(shift_register, encrypted_register, &aes_key);
        
        // XOR только с оставшимися байтами
        xor_blocks(plaintext + i, ciphertext + i, encrypted_register, remaining);
    }

    *output_size = ciphertext_len;
    return plaintext;
}

