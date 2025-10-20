#ifndef CSPRNG_H
#define CSPRNG_H

#include <stddef.h>

/**
 * Генерация криптографически стойких случайных байтов
 * 
 * @param buffer Буфер для записи случайных байтов
 * @param num_bytes Количество байтов для генерации
 * @return 0 в случае успеха, -1 в случае ошибки
 */
int generate_random_bytes(unsigned char *buffer, size_t num_bytes);

/**
 * Генерация случайного ключа для AES-128
 * 
 * @param key Буфер для записи ключа (16 байт)
 * @return 0 в случае успеха, -1 в случае ошибки
 */
int generate_aes_key(unsigned char *key);

/**
 * Генерация случайного IV для режимов с IV
 * 
 * @param iv Буфер для записи IV (16 байт)
 * @return 0 в случае успеха, -1 в случае ошибки
 */
int generate_random_iv(unsigned char *iv);

/**
 * Проверка слабых ключей
 * 
 * @param key Ключ для проверки
 * @param key_len Длина ключа в байтах
 * @return 1 если ключ слабый, 0 если ключ нормальный
 */
int is_weak_key(const unsigned char *key, size_t key_len);

#endif // CSPRNG_H
