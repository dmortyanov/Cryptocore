#include "../../include/modes.h"
#include "../../include/ecb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>

unsigned char* aes_ofb_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size) {
    // OFB - потоковый шифр, дополнение не требуется
    unsigned char* ciphertext = (unsigned char*)malloc(plaintext_len);
    if (!ciphertext) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        return NULL;
    }

    // Настройка ключа AES для шифрования
    AES_KEY aes_key;
    if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
        fprintf(stderr, "Error: Failed to set AES encryption key\n");
        free(ciphertext);
        return NULL;
    }

    // Регистр обратной связи (начинается с IV)
    unsigned char feedback[AES_BLOCK_SIZE];
    memcpy(feedback, iv, AES_BLOCK_SIZE);

    size_t i = 0;
    // Обработка полных блоков
    while (i + AES_BLOCK_SIZE <= plaintext_len) {
        unsigned char keystream[AES_BLOCK_SIZE];
        
        // Шифрование регистра обратной связи для генерации потока ключа
        AES_encrypt(feedback, keystream, &aes_key);
        
        // XOR потока ключа с блоком открытого текста
        xor_blocks(ciphertext + i, plaintext + i, keystream, AES_BLOCK_SIZE);
        
        // Обновление регистра обратной связи (становится потоком ключа)
        memcpy(feedback, keystream, AES_BLOCK_SIZE);
        
        i += AES_BLOCK_SIZE;
    }

    // Обработка последнего неполного блока (если есть)
    if (i < plaintext_len) {
        unsigned char keystream[AES_BLOCK_SIZE];
        size_t remaining = plaintext_len - i;
        
        // Шифрование регистра обратной связи
        AES_encrypt(feedback, keystream, &aes_key);
        
        // XOR только с оставшимися байтами
        xor_blocks(ciphertext + i, plaintext + i, keystream, remaining);
    }

    *output_size = plaintext_len;
    return ciphertext;
}

unsigned char* aes_ofb_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size) {
    // OFB - шифрование и дешифрование идентичны (XOR с тем же потоком ключа)
    return aes_ofb_encrypt(ciphertext, ciphertext_len, key, iv, output_size);
}

