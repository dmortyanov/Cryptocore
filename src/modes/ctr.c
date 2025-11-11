#include "../../include/modes.h"
#include "../../include/ecb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>

/**
 * Инкремент счетчика (big-endian)
 */
static void increment_counter(unsigned char* counter) {
    // Инкремент с конца (big-endian)
    for (int i = AES_BLOCK_SIZE - 1; i >= 0; i--) {
        if (++counter[i] != 0) {
            break;  // Нет переноса
        }
        // Продолжаем, если был перенос
    }
}

unsigned char* aes_ctr_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size) {
    // CTR - потоковый шифр, дополнение не требуется
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

    // Инициализация счетчика из IV
    unsigned char counter[AES_BLOCK_SIZE];
    memcpy(counter, iv, AES_BLOCK_SIZE);

    size_t i = 0;
    // Обработка полных блоков
    while (i + AES_BLOCK_SIZE <= plaintext_len) {
        unsigned char keystream[AES_BLOCK_SIZE];
        
        // Шифрование счетчика для генерации потока ключа
        AES_encrypt(counter, keystream, &aes_key);
        
        // XOR потока ключа с блоком открытого текста
        xor_blocks(ciphertext + i, plaintext + i, keystream, AES_BLOCK_SIZE);
        
        // Инкремент счетчика
        increment_counter(counter);
        
        i += AES_BLOCK_SIZE;
    }

    // Обработка последнего неполного блока (если есть)
    if (i < plaintext_len) {
        unsigned char keystream[AES_BLOCK_SIZE];
        size_t remaining = plaintext_len - i;
        
        // Шифрование счетчика
        AES_encrypt(counter, keystream, &aes_key);
        
        // XOR только с оставшимися байтами
        xor_blocks(ciphertext + i, plaintext + i, keystream, remaining);
    }

    *output_size = plaintext_len;
    return ciphertext;
}

unsigned char* aes_ctr_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size) {
    // CTR - шифрование и дешифрование идентичны (XOR с тем же потоком ключа)
    return aes_ctr_encrypt(ciphertext, ciphertext_len, key, iv, output_size);
}

