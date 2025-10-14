#include "../include/file_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char* read_file(const char* filename, size_t* size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Ошибка: Не удается открыть входной файл '%s'\n", filename);
        return NULL;
    }

    // Определение размера файла
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fprintf(stderr, "Ошибка: Не удается определить размер файла '%s'\n", filename);
        fclose(file);
        return NULL;
    }

    // Выделение буфера
    unsigned char* buffer = (unsigned char*)malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Ошибка: Не удалось выделить память\n");
        fclose(file);
        return NULL;
    }

    // Чтение файла
    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "Ошибка: Не удалось прочитать весь файл '%s'\n", filename);
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *size = file_size;
    return buffer;
}

int write_file(const char* filename, const unsigned char* data, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Ошибка: Не удается открыть выходной файл '%s'\n", filename);
        return -1;
    }

    size_t bytes_written = fwrite(data, 1, size, file);
    if (bytes_written != size) {
        fprintf(stderr, "Ошибка: Не удалось записать весь файл '%s'\n", filename);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}
