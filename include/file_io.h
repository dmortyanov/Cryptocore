#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h>

/**
 * Чтение всего файла в память
 * Возвращает указатель на выделенный буфер, устанавливает size
 * Вызывающая сторона должна освободить возвращенный буфер
 */
unsigned char* read_file(const char* filename, size_t* size);

/**
 * Запись буфера в файл
 * Возвращает 0 при успехе, -1 при ошибке
 */
int write_file(const char* filename, const unsigned char* data, size_t size);

#endif /* FILE_IO_H */
