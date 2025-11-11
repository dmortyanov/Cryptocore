#include "../include/file_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char* read_file(const char* filename, size_t* size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Failed to open input file '%s'\n", filename);
        return NULL;
    }

    // Определение размера файла (64-bit)
#if defined(_WIN32)
    _fseeki64(file, 0, SEEK_END);
    long long file_size = _ftelli64(file);
    _fseeki64(file, 0, SEEK_SET);
#else
    fseeko(file, 0, SEEK_END);
    off_t file_size = ftello(file);
    fseeko(file, 0, SEEK_SET);
#endif

    if (file_size < 0) {
        fprintf(stderr, "Error: Failed to determine file size of '%s'\n", filename);
        fclose(file);
        return NULL;
    }

    // Выделение буфера
    unsigned char* buffer = (unsigned char*)malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        fclose(file);
        return NULL;
    }

    // Чтение файла
    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read entire file '%s'\n", filename);
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
        fprintf(stderr, "Error: Failed to open output file '%s'\n", filename);
        return -1;
    }

    size_t bytes_written = fwrite(data, 1, size, file);
    if (bytes_written != size) {
        fprintf(stderr, "Error: Failed to write entire file '%s'\n", filename);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}
