#ifndef ECB_H
#define ECB_H

#include <stddef.h>

#define AES_BLOCK_SIZE 16
#define AES_128_KEY_SIZE 16

/**
 * Шифрование данных с использованием AES-128 в режиме ECB с дополнением PKCS#7
 * Возвращает указатель на выделенный буфер с зашифрованными данными, устанавливает output_size
 * Вызывающая сторона должна освободить возвращенный буфер
 */
unsigned char* aes_ecb_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, size_t* output_size);

/**
 * Дешифрование данных с использованием AES-128 в режиме ECB и удаление дополнения PKCS#7
 * Возвращает указатель на выделенный буфер с дешифрованными данными, устанавливает output_size
 * Вызывающая сторона должна освободить возвращенный буфер
 */
unsigned char* aes_ecb_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, size_t* output_size);

/**
 * Преобразование hex-строки в байты
 * Возвращает указатель на выделенный массив байтов, устанавливает size
 * Вызывающая сторона должна освободить возвращенный буфер
 */
unsigned char* hex_to_bytes(const char* hex_string, size_t* size);

#endif /* ECB_H */
