#ifndef MODES_H
#define MODES_H

#include <stddef.h>
#include "ecb.h"

/**
 * Режим CBC (Cipher Block Chaining)
 * Шифрование данных с использованием AES-128 в режиме CBC с дополнением PKCS#7
 * Возвращает указатель на выделенный буфер с зашифрованными данными, устанавливает output_size
 * Вызывающая сторона должна освободить возвращенный буфер
 */
unsigned char* aes_cbc_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size);

/**
 * Дешифрование данных с использованием AES-128 в режиме CBC и удаление дополнения PKCS#7
 * Возвращает указатель на выделенный буфер с дешифрованными данными, устанавливает output_size
 * Вызывающая сторона должна освободить возвращенный буфер
 */
unsigned char* aes_cbc_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size);

/**
 * Режим CFB (Cipher Feedback)
 * Шифрование данных с использованием AES-128 в режиме CFB (полный блок)
 * Не требует дополнения
 */
unsigned char* aes_cfb_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size);

/**
 * Дешифрование данных с использованием AES-128 в режиме CFB
 */
unsigned char* aes_cfb_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size);

/**
 * Режим OFB (Output Feedback)
 * Шифрование данных с использованием AES-128 в режиме OFB
 * Не требует дополнения
 */
unsigned char* aes_ofb_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size);

/**
 * Дешифрование данных с использованием AES-128 в режиме OFB
 */
unsigned char* aes_ofb_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size);

/**
 * Режим CTR (Counter)
 * Шифрование данных с использованием AES-128 в режиме CTR
 * Не требует дополнения
 */
unsigned char* aes_ctr_encrypt(const unsigned char* plaintext, size_t plaintext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size);

/**
 * Дешифрование данных с использованием AES-128 в режиме CTR
 */
unsigned char* aes_ctr_decrypt(const unsigned char* ciphertext, size_t ciphertext_len,
                                const unsigned char* key, const unsigned char* iv,
                                size_t* output_size);

/**
 * Генерация криптографически стойкого случайного IV (16 байт)
 * Возвращает указатель на выделенный буфер с IV
 * Вызывающая сторона должна освободить возвращенный буфер
 */
unsigned char* generate_iv(void);

/**
 * XOR двух буферов
 * output = a XOR b
 */
void xor_blocks(unsigned char* output, const unsigned char* a, const unsigned char* b, size_t len);

#endif /* MODES_H */

