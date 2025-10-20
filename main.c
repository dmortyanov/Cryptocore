#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "include/ecb.h"
#include "include/modes.h"
#include "include/file_io.h"
#include "include/mouse_entropy.h"
#include "include/csprng.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#define stat _stat
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#else
#include <sys/stat.h>
#include <dirent.h>
#endif

typedef struct {
    char* algorithm;
    char* mode;
    int encrypt;
    int decrypt;
    char* key_hex;
    int use_mouse_key;
    char* iv_hex;
    char* input_path;
    char* output_path;
    int recursive;
} cli_args_t;

typedef struct {
    char* original_name;
    char* encrypted_name;
} filename_mapping_t;

// Объявления функций
int encrypt_single_file(cli_args_t* args, unsigned char* key, const char* key_hex);
int decrypt_single_file(cli_args_t* args, unsigned char* key, const char* key_hex);
int encrypt_directory(cli_args_t* args, unsigned char* key, const char* key_hex);
int decrypt_directory(cli_args_t* args, unsigned char* key, const char* key_hex);

/**
 * Вывод информации об использовании программы
 */
void print_usage(const char* program_name) {
    fprintf(stderr, "Использование: %s [ОПЦИИ]\n\n", program_name);
    fprintf(stderr, "Обязательные опции:\n");
    fprintf(stderr, "  --algorithm АЛГОРИТМ   Алгоритм шифрования (поддерживается: aes)\n");
    fprintf(stderr, "  --mode РЕЖИМ           Режим работы (ecb, cbc, cfb, ofb, ctr)\n");
    fprintf(stderr, "  --encrypt              Выполнить шифрование\n");
    fprintf(stderr, "  --decrypt              Выполнить дешифрование\n");
    fprintf(stderr, "  --key КЛЮЧ             Ключ шифрования/дешифрования (hex-строка, 32 символа для AES-128)\n");
    fprintf(stderr, "  --input ФАЙЛ           Путь к входному файлу\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Опциональные:\n");
    fprintf(stderr, "  --output ФАЙЛ          Путь к выходному файлу (по умолчанию: <input>.enc или <input>.dec)\n");
    fprintf(stderr, "  --iv IV                Вектор инициализации (только для дешифрования, hex-строка, 32 символа)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Примечания:\n");
    fprintf(stderr, "  - При шифровании автоматически генерируется ключ по движению мыши\n");
    fprintf(stderr, "  - При шифровании директории программа запросит новые имена для каждого файла\n");
    fprintf(stderr, "  - Для режимов ecb: IV не используется\n");
    fprintf(stderr, "  - Для режимов cbc, cfb, ofb, ctr:\n");
    fprintf(stderr, "    * При шифровании: IV генерируется автоматически и добавляется в начало файла\n");
    fprintf(stderr, "    * При дешифровании: IV читается из начала файла или указывается через --iv\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Примеры:\n");
    fprintf(stderr, "  Шифрование директории:\n");
    fprintf(stderr, "    %s --algorithm aes --mode cbc --encrypt --input ./files --output ./encryptfiles\n\n", program_name);
    fprintf(stderr, "  Дешифрование директории:\n");
    fprintf(stderr, "    %s --algorithm aes --mode cbc --decrypt --input ./encryptfiles --output ./decryptfiles\n", program_name);
}

/**
 * Разбор аргументов командной строки
 */
int parse_args(int argc, char* argv[], cli_args_t* args) {
    // Инициализация аргументов
    memset(args, 0, sizeof(cli_args_t));

    // Разбор аргументов
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--algorithm") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Ошибка: --algorithm требует аргумент\n");
                return -1;
            }
            args->algorithm = argv[++i];
        } else if (strcmp(argv[i], "--mode") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Ошибка: --mode требует аргумент\n");
                return -1;
            }
            args->mode = argv[++i];
        } else if (strcmp(argv[i], "--encrypt") == 0) {
            args->encrypt = 1;
        } else if (strcmp(argv[i], "--decrypt") == 0) {
            args->decrypt = 1;
        } else if (strcmp(argv[i], "--key") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Ошибка: --key требует аргумент\n");
                return -1;
            }
            args->key_hex = argv[++i];
        } else if (strcmp(argv[i], "--iv") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Ошибка: --iv требует аргумент\n");
                return -1;
            }
            args->iv_hex = argv[++i];
        } else if (strcmp(argv[i], "--input") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Ошибка: --input требует аргумент\n");
                return -1;
            }
            args->input_path = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Ошибка: --output требует аргумент\n");
                return -1;
            }
            args->output_path = argv[++i];
        } else {
            fprintf(stderr, "Ошибка: Неизвестный аргумент '%s'\n", argv[i]);
            return -1;
        }
    }

    return 0;
}

/**
 * Проверка, требуется ли IV для данного режима
 */
int mode_requires_iv(const char* mode) {
    return strcmp(mode, "cbc") == 0 ||
           strcmp(mode, "cfb") == 0 ||
           strcmp(mode, "ofb") == 0 ||
           strcmp(mode, "ctr") == 0;
}

/**
 * Проверка корректности аргументов
 */
int validate_args(cli_args_t* args) {
    // Проверка обязательных аргументов
    if (!args->algorithm) {
        fprintf(stderr, "Ошибка: --algorithm обязателен\n");
        return -1;
    }
    if (!args->mode) {
        fprintf(stderr, "Ошибка: --mode обязателен\n");
        return -1;
    }
    if (args->encrypt) {
        args->use_mouse_key = 1;  // Автоматически используем генерацию ключа по мыши при шифровании
    }
    
    // Sprint 3: --key теперь опциональный для шифрования
    if (!args->key_hex && !args->use_mouse_key && !args->encrypt) {
        fprintf(stderr, "Ошибка: --key обязателен при дешифровании\n");
        return -1;
    }
    if (!args->input_path) {
        fprintf(stderr, "Ошибка: --input обязателен\n");
        return -1;
    }
    
    // Генерация имени выходного файла по умолчанию если не указан
    if (!args->output_path) {
        size_t len = strlen(args->input_path);
        args->output_path = (char*)malloc(len + 5);
        if (args->encrypt) {
            sprintf(args->output_path, "%s.enc", args->input_path);
        } else {
            sprintf(args->output_path, "%s.dec", args->input_path);
        }
    }

    // Проверка флагов шифрования/дешифрования
    if (args->encrypt && args->decrypt) {
        fprintf(stderr, "Ошибка: Нельзя указывать одновременно --encrypt и --decrypt\n");
        return -1;
    }
    if (!args->encrypt && !args->decrypt) {
        fprintf(stderr, "Ошибка: Необходимо указать --encrypt или --decrypt\n");
        return -1;
    }

    if (args->use_mouse_key) {
#ifndef _WIN32
        fprintf(stderr, "Ошибка: Генерация ключа по мыши доступна только в Windows-сборке\n");
        return -1;
#else
        if (args->key_hex) {
            fprintf(stderr, "Предупреждение: --key игнорируется при автоматической генерации ключа\n");
            args->key_hex = NULL;
        }
#endif
    }
    
    // Проверка IV согласно Sprint 2 требованиям
    int needs_iv = mode_requires_iv(args->mode);
    
    if (needs_iv) {
        if (args->encrypt && args->iv_hex) {
            fprintf(stderr, "Предупреждение: --iv игнорируется при шифровании (IV генерируется автоматически)\n");
            args->iv_hex = NULL;
        }
        // При дешифровании IV может быть не указан - будет читаться из файла
    }

    // Проверка алгоритма
    if (strcmp(args->algorithm, "aes") != 0) {
        fprintf(stderr, "Ошибка: Неподдерживаемый алгоритм '%s'. Поддерживается только 'aes'.\n", args->algorithm);
        return -1;
    }

    // Проверка режима
    if (strcmp(args->mode, "ecb") != 0 && strcmp(args->mode, "cbc") != 0 &&
        strcmp(args->mode, "cfb") != 0 && strcmp(args->mode, "ofb") != 0 &&
        strcmp(args->mode, "ctr") != 0) {
        fprintf(stderr, "Ошибка: Неподдерживаемый режим '%s'. Поддерживаются: ecb, cbc, cfb, ofb, ctr.\n", args->mode);
        return -1;
    }

    // Проверка длины ключа
    if (args->key_hex) {
    size_t key_len = strlen(args->key_hex);
    if (key_len != 32) {
        fprintf(stderr, "Ошибка: Ключ AES-128 должен быть 32 шестнадцатеричных символа (16 байт)\n");
        fprintf(stderr, "       Предоставлена длина ключа: %zu символов\n", key_len);
        return -1;
        }
    }

    // Проверка IV
    if (args->iv_hex) {
        if (args->encrypt) {
            fprintf(stderr, "Предупреждение: --iv игнорируется при шифровании (IV генерируется автоматически)\n");
            args->iv_hex = NULL;
        } else {
            // Проверка длины IV
            size_t iv_len = strlen(args->iv_hex);
            if (iv_len != 32) {
                fprintf(stderr, "Ошибка: IV должен быть 32 шестнадцатеричных символа (16 байт)\n");
                fprintf(stderr, "       Предоставлена длина IV: %zu символов\n", iv_len);
                return -1;
            }
        }
    }

    return 0;
}

/**
 * Проверка, является ли путь директорией
 */
int is_directory(const char* path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

/**
 * Создание директории, если она не существует
 */
int ensure_directory_exists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
#ifdef _WIN32
        return _mkdir(path);
#else
        return mkdir(path, 0755);
#endif
    }
    return 0;
}

/**
 * Получение списка файлов в директории
 */
char** get_files_in_directory(const char* dir_path, int* file_count) {
    char** files = NULL;
    *file_count = 0;
    
#ifdef _WIN32
    WIN32_FIND_DATA find_data;
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
    
    HANDLE hFind = FindFirstFile(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            files = realloc(files, (*file_count + 1) * sizeof(char*));
            files[*file_count] = malloc(strlen(find_data.cFileName) + 1);
            strcpy(files[*file_count], find_data.cFileName);
            (*file_count)++;
        }
    } while (FindNextFile(hFind, &find_data));
    
    FindClose(hFind);
#else
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return NULL;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // Обычный файл
            files = realloc(files, (*file_count + 1) * sizeof(char*));
            files[*file_count] = malloc(strlen(entry->d_name) + 1);
            strcpy(files[*file_count], entry->d_name);
            (*file_count)++;
        }
    }
    
    closedir(dir);
#endif
    
    return files;
}

/**
 * Освобождение списка файлов
 */
void free_file_list(char** files, int file_count) {
    for (int i = 0; i < file_count; i++) {
        free(files[i]);
    }
    free(files);
}

/**
 * Запрос нового имени файла у пользователя
 */
char* get_new_filename(const char* original_name) {
    char* new_name = malloc(256);
    printf("Введите имя нового файла для '%s': ", original_name);
    if (fgets(new_name, 256, stdin) == NULL) {
        free(new_name);
        return NULL;
    }
    
    // Удаляем символ новой строки
    size_t len = strlen(new_name);
    if (len > 0 && new_name[len-1] == '\n') {
        new_name[len-1] = '\0';
    }
    
    return new_name;
}

/**
 * Запись метаданных о файлах в файл
 */
int write_metadata_file(const char* output_dir, filename_mapping_t* mappings, int count, const char* key_hex) {
    char metadata_path[512];
#ifdef _WIN32
    snprintf(metadata_path, sizeof(metadata_path), "%s\\metadata.txt", output_dir);
#else
    snprintf(metadata_path, sizeof(metadata_path), "%s/metadata.txt", output_dir);
#endif
    
    FILE* file = fopen(metadata_path, "w");
    if (!file) {
        return -1;
    }
    
    fprintf(file, "KEY=%s\n", key_hex);
    fprintf(file, "COUNT=%d\n", count);
    
    for (int i = 0; i < count; i++) {
        fprintf(file, "%s|%s\n", mappings[i].original_name, mappings[i].encrypted_name);
    }
    
    fclose(file);
    return 0;
}

/**
 * Чтение метаданных из файла
 */
filename_mapping_t* read_metadata_file(const char* input_dir, int* count, char** key_hex) {
    char metadata_path[512];
#ifdef _WIN32
    snprintf(metadata_path, sizeof(metadata_path), "%s\\metadata.txt", input_dir);
#else
    snprintf(metadata_path, sizeof(metadata_path), "%s/metadata.txt", input_dir);
#endif
    
    FILE* file = fopen(metadata_path, "r");
    if (!file) {
        return NULL;
    }
    
    char line[512];
    filename_mapping_t* mappings = NULL;
    *count = 0;
    
    // Читаем ключ
    if (fgets(line, sizeof(line), file) && strncmp(line, "KEY=", 4) == 0) {
        *key_hex = malloc(strlen(line) - 4);
        strcpy(*key_hex, line + 4);
        // Удаляем символ новой строки
        size_t len = strlen(*key_hex);
        if (len > 0 && (*key_hex)[len-1] == '\n') {
            (*key_hex)[len-1] = '\0';
        }
    }
    
    // Читаем количество файлов
    if (fgets(line, sizeof(line), file) && strncmp(line, "COUNT=", 6) == 0) {
        *count = atoi(line + 6);
    }
    
    // Читаем маппинги файлов
    mappings = malloc(*count * sizeof(filename_mapping_t));
    for (int i = 0; i < *count && fgets(line, sizeof(line), file); i++) {
        char* separator = strchr(line, '|');
        if (separator) {
            *separator = '\0';
            mappings[i].original_name = malloc(strlen(line) + 1);
            strcpy(mappings[i].original_name, line);
            
            mappings[i].encrypted_name = malloc(strlen(separator + 1) + 1);
            strcpy(mappings[i].encrypted_name, separator + 1);
            
            // Удаляем символ новой строки
            size_t len = strlen(mappings[i].encrypted_name);
            if (len > 0 && mappings[i].encrypted_name[len-1] == '\n') {
                mappings[i].encrypted_name[len-1] = '\0';
            }
        }
    }
    
    fclose(file);
    return mappings;
}

/**
 * Освобождение метаданных
 */
void free_metadata(filename_mapping_t* mappings, int count, char* key_hex) {
    for (int i = 0; i < count; i++) {
        free(mappings[i].original_name);
        free(mappings[i].encrypted_name);
    }
    free(mappings);
    if (key_hex) {
        free(key_hex);
    }
}

int main(int argc, char* argv[]) {
    cli_args_t args;
    unsigned char* key = NULL;
    char* key_hex = NULL;
    int result = 1;

    // Разбор и проверка аргументов
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (parse_args(argc, argv, &args) != 0) {
        fprintf(stderr, "\n");
        print_usage(argv[0]);
        return 1;
    }

    if (validate_args(&args) != 0) {
        fprintf(stderr, "\n");
        print_usage(argv[0]);
        return 1;
    }

    // Генерация ключа согласно Sprint 3 требованиям
    if (args.use_mouse_key) {
#ifdef _WIN32
        key = (unsigned char*)malloc(AES_128_KEY_SIZE);
        if (!key) {
            fprintf(stderr, "Ошибка: Не удалось выделить память для ключа\n");
            goto cleanup;
        }
        printf("Генерация ключа по движению мыши...\n");
        if (generate_mouse_key(key, AES_128_KEY_SIZE) != 0) {
            fprintf(stderr, "Ошибка: Не удалось сгенерировать ключ на основе движения мыши\n");
            goto cleanup;
        }
        printf("Ключ успешно сгенерирован.\n");
        
        // Преобразуем ключ в hex для сохранения
        key_hex = malloc(AES_128_KEY_SIZE * 2 + 1);
        for (int i = 0; i < AES_128_KEY_SIZE; i++) {
            sprintf(key_hex + i * 2, "%02x", key[i]);
        }
        printf("Ключ (hex): %s\n", key_hex);
#else
        fprintf(stderr, "Ошибка: Генерация ключа по мыши доступна только в Windows\n");
        goto cleanup;
#endif
    } else if (args.key_hex) {
        // Используем предоставленный ключ
        key_hex = malloc(strlen(args.key_hex) + 1);
        strcpy(key_hex, args.key_hex);
        
        size_t key_size;
    key = hex_to_bytes(args.key_hex, &key_size);
    if (!key) {
        goto cleanup;
    }
    if (key_size != AES_128_KEY_SIZE) {
        fprintf(stderr, "Ошибка: Ключ должен быть ровно %d байт для AES-128\n", AES_128_KEY_SIZE);
        goto cleanup;
    }

        // Проверка на слабые ключи
        if (is_weak_key(key, AES_128_KEY_SIZE)) {
            fprintf(stderr, "Предупреждение: Обнаружен слабый ключ. Рекомендуется использовать криптографически стойкий ключ.\n");
        }
    } else if (args.encrypt) {
        // Sprint 3: Генерация случайного ключа при шифровании
        key = (unsigned char*)malloc(AES_128_KEY_SIZE);
        if (!key) {
            fprintf(stderr, "Ошибка: Не удалось выделить память для ключа\n");
            goto cleanup;
        }
        
        if (generate_aes_key(key) != 0) {
            fprintf(stderr, "Ошибка: Не удалось сгенерировать криптографически стойкий ключ\n");
            goto cleanup;
        }
        
        // Преобразуем ключ в hex для вывода
        key_hex = malloc(AES_128_KEY_SIZE * 2 + 1);
        for (int i = 0; i < AES_128_KEY_SIZE; i++) {
            sprintf(key_hex + i * 2, "%02x", key[i]);
        }
        
        // Выводим ключ согласно Sprint 3 требованиям
        printf("[INFO] Generated random key: %s\n", key_hex);
    } else {
        // При дешифровании ключ обязателен
        fprintf(stderr, "Ошибка: --key обязателен при дешифровании\n");
        goto cleanup;
    }

    // Проверяем, является ли входной путь директорией
    if (is_directory(args.input_path)) {
        if (args.encrypt) {
            result = encrypt_directory(&args, key, key_hex);
        } else {
            result = decrypt_directory(&args, key, key_hex);
        }
    } else {
        // Обработка одного файла
        if (args.encrypt) {
            result = encrypt_single_file(&args, key, key_hex);
        } else {
            result = decrypt_single_file(&args, key, key_hex);
        }
    }

cleanup:
    if (key) free(key);
    if (key_hex) free(key_hex);
    return result;
}

/**
 * Шифрование одного файла
 */
int encrypt_single_file(cli_args_t* args, unsigned char* key, const char* key_hex) {
    unsigned char* input_data = NULL;
    unsigned char* output_data = NULL;
    unsigned char* iv = NULL;
    size_t input_size, output_size;
    int result = 1;
    int needs_iv = mode_requires_iv(args->mode);
    
    (void)key_hex;  // Не используется в функции одиночного файла

    // Чтение входного файла
    input_data = read_file(args->input_path, &input_size);
    if (!input_data) {
        goto cleanup;
    }

    // Sprint 3: Генерация IV с использованием CSPRNG
    if (needs_iv) {
        iv = malloc(AES_BLOCK_SIZE);
        if (!iv) {
            goto cleanup;
        }
        if (generate_random_iv(iv) != 0) {
            fprintf(stderr, "Ошибка: Не удалось сгенерировать криптографически стойкий IV\n");
            goto cleanup;
        }
    }

    // Шифрование
    printf("Шифрование '%s' -> '%s' (режим: %s)\n", args->input_path, args->output_path, args->mode);
    
    if (strcmp(args->mode, "ecb") == 0) {
            output_data = aes_ecb_encrypt(input_data, input_size, key, &output_size);
    } else if (strcmp(args->mode, "cbc") == 0) {
            output_data = aes_cbc_encrypt(input_data, input_size, key, iv, &output_size);
    } else if (strcmp(args->mode, "cfb") == 0) {
            output_data = aes_cfb_encrypt(input_data, input_size, key, iv, &output_size);
    } else if (strcmp(args->mode, "ofb") == 0) {
            output_data = aes_ofb_encrypt(input_data, input_size, key, iv, &output_size);
    } else if (strcmp(args->mode, "ctr") == 0) {
            output_data = aes_ctr_encrypt(input_data, input_size, key, iv, &output_size);
    }

    if (!output_data) {
        goto cleanup;
    }

    // Добавляем IV в начало файла для режимов с IV
    if (needs_iv) {
        unsigned char* final_output = malloc(AES_BLOCK_SIZE + output_size);
        if (!final_output) {
            goto cleanup;
        }
        
        memcpy(final_output, iv, AES_BLOCK_SIZE);
        memcpy(final_output + AES_BLOCK_SIZE, output_data, output_size);
        
        free(output_data);
        output_data = final_output;
        output_size += AES_BLOCK_SIZE;
    }

    // Запись выходного файла
    if (write_file(args->output_path, output_data, output_size) != 0) {
        goto cleanup;
    }

    printf("Успешно! Обработано %zu байт -> %zu байт\n", input_size, output_size);
    result = 0;

cleanup:
    if (iv) free(iv);
    if (input_data) free(input_data);
    if (output_data) free(output_data);
    return result;
}

/**
 * Дешифрование одного файла
 */
int decrypt_single_file(cli_args_t* args, unsigned char* key, const char* key_hex) {
    unsigned char* input_data = NULL;
    unsigned char* output_data = NULL;
    unsigned char* iv = NULL;
    size_t input_size, output_size;
    int result = 1;
    int needs_iv = mode_requires_iv(args->mode);
    
    (void)key_hex;  // Не используется в функции одиночного файла

    // Чтение входного файла
    input_data = read_file(args->input_path, &input_size);
    if (!input_data) {
        goto cleanup;
    }

    // Извлечение IV из файла для режимов с IV
    if (needs_iv) {
        if (args->iv_hex) {
            // IV предоставлен через командную строку
            size_t iv_size;
            iv = hex_to_bytes(args->iv_hex, &iv_size);
            if (!iv || iv_size != AES_BLOCK_SIZE) {
                fprintf(stderr, "Ошибка: Неверный IV\n");
                goto cleanup;
            }
        } else {
            // IV читается из начала файла
            if (input_size < AES_BLOCK_SIZE) {
                fprintf(stderr, "Ошибка: Файл слишком мал, чтобы содержать IV\n");
                goto cleanup;
            }
            
            iv = malloc(AES_BLOCK_SIZE);
            if (!iv) {
                goto cleanup;
            }
            memcpy(iv, input_data, AES_BLOCK_SIZE);
            
            // Убираем IV из входных данных
            unsigned char* temp = malloc(input_size - AES_BLOCK_SIZE);
            if (!temp) {
                goto cleanup;
            }
            memcpy(temp, input_data + AES_BLOCK_SIZE, input_size - AES_BLOCK_SIZE);
            free(input_data);
            input_data = temp;
            input_size -= AES_BLOCK_SIZE;
        }
    }

    // Дешифрование
    printf("Дешифрование '%s' -> '%s' (режим: %s)\n", args->input_path, args->output_path, args->mode);
    
    if (strcmp(args->mode, "ecb") == 0) {
        output_data = aes_ecb_decrypt(input_data, input_size, key, &output_size);
    } else if (strcmp(args->mode, "cbc") == 0) {
        output_data = aes_cbc_decrypt(input_data, input_size, key, iv, &output_size);
    } else if (strcmp(args->mode, "cfb") == 0) {
        output_data = aes_cfb_decrypt(input_data, input_size, key, iv, &output_size);
    } else if (strcmp(args->mode, "ofb") == 0) {
        output_data = aes_ofb_decrypt(input_data, input_size, key, iv, &output_size);
    } else if (strcmp(args->mode, "ctr") == 0) {
        output_data = aes_ctr_decrypt(input_data, input_size, key, iv, &output_size);
    }

    if (!output_data) {
        goto cleanup;
    }

    // Запись выходного файла
    if (write_file(args->output_path, output_data, output_size) != 0) {
        goto cleanup;
    }

    printf("Успешно! Обработано %zu байт -> %zu байт\n", input_size, output_size);
    result = 0;

cleanup:
    if (iv) free(iv);
    if (input_data) free(input_data);
    if (output_data) free(output_data);
    return result;
}

/**
 * Шифрование директории
 */
int encrypt_directory(cli_args_t* args, unsigned char* key, const char* key_hex) {
    int file_count;
    char** files = get_files_in_directory(args->input_path, &file_count);
    if (!files) {
        fprintf(stderr, "Ошибка: Не удалось получить список файлов в директории '%s'\n", args->input_path);
        return 1;
    }

    if (file_count == 0) {
        printf("Директория '%s' пуста\n", args->input_path);
        free_file_list(files, file_count);
        return 0;
    }

    // Создаем выходную директорию
    if (ensure_directory_exists(args->output_path) != 0) {
        fprintf(stderr, "Ошибка: Не удалось создать выходную директорию '%s'\n", args->output_path);
        free_file_list(files, file_count);
        return 1;
    }

    filename_mapping_t* mappings = malloc(file_count * sizeof(filename_mapping_t));
    int success_count = 0;

    printf("Найдено %d файлов для шифрования\n", file_count);

    for (int i = 0; i < file_count; i++) {
        char input_file_path[512];
        char output_file_path[512];
        
#ifdef _WIN32
        snprintf(input_file_path, sizeof(input_file_path), "%s\\%s", args->input_path, files[i]);
#else
        snprintf(input_file_path, sizeof(input_file_path), "%s/%s", args->input_path, files[i]);
#endif
        
        // Запрашиваем новое имя файла
        char* new_name = get_new_filename(files[i]);
        if (!new_name) {
            fprintf(stderr, "Ошибка: Не удалось получить новое имя для файла '%s'\n", files[i]);
            continue;
        }
        
#ifdef _WIN32
        snprintf(output_file_path, sizeof(output_file_path), "%s\\%s", args->output_path, new_name);
#else
        snprintf(output_file_path, sizeof(output_file_path), "%s/%s", args->output_path, new_name);
#endif
        
        // Сохраняем маппинг
        mappings[success_count].original_name = malloc(strlen(files[i]) + 1);
        strcpy(mappings[success_count].original_name, files[i]);
        mappings[success_count].encrypted_name = malloc(strlen(new_name) + 1);
        strcpy(mappings[success_count].encrypted_name, new_name);
        
        // Создаем временные аргументы для обработки файла
        cli_args_t temp_args = *args;
        temp_args.input_path = input_file_path;
        temp_args.output_path = output_file_path;
        
        if (encrypt_single_file(&temp_args, key, key_hex) == 0) {
            success_count++;
        }
        
        free(new_name);
    }

    // Записываем метаданные
    if (success_count > 0) {
        write_metadata_file(args->output_path, mappings, success_count, key_hex);
        printf("Зашифровано %d из %d файлов\n", success_count, file_count);
    }

    // Освобождаем память
    for (int i = 0; i < success_count; i++) {
        free(mappings[i].original_name);
        free(mappings[i].encrypted_name);
    }
    free(mappings);
    free_file_list(files, file_count);

    return success_count == file_count ? 0 : 1;
}

/**
 * Дешифрование директории
 */
int decrypt_directory(cli_args_t* args, unsigned char* key, const char* key_hex) {
    int file_count;
    char** files = get_files_in_directory(args->input_path, &file_count);
    if (!files) {
        fprintf(stderr, "Ошибка: Не удалось получить список файлов в директории '%s'\n", args->input_path);
        return 1;
    }

    // Читаем метаданные
    int mapping_count;
    char* metadata_key_hex;
    filename_mapping_t* mappings = read_metadata_file(args->input_path, &mapping_count, &metadata_key_hex);
    
    if (!mappings) {
        fprintf(stderr, "Ошибка: Не удалось прочитать метаданные из директории '%s'\n", args->input_path);
        free_file_list(files, file_count);
        return 1;
    }

    // Проверяем ключ
    if (strcmp(key_hex, metadata_key_hex) != 0) {
        fprintf(stderr, "Ошибка: Ключ не совпадает с ключом, использованным при шифровании\n");
        free_metadata(mappings, mapping_count, metadata_key_hex);
        free_file_list(files, file_count);
        return 1;
    }

    // Создаем выходную директорию
    if (ensure_directory_exists(args->output_path) != 0) {
        fprintf(stderr, "Ошибка: Не удалось создать выходную директорию '%s'\n", args->output_path);
        free_metadata(mappings, mapping_count, metadata_key_hex);
        free_file_list(files, file_count);
        return 1;
    }

    int success_count = 0;

    printf("Найдено %d зашифрованных файлов для дешифрования\n", mapping_count);

    for (int i = 0; i < mapping_count; i++) {
        char input_file_path[512];
        char output_file_path[512];
        
#ifdef _WIN32
        snprintf(input_file_path, sizeof(input_file_path), "%s\\%s", args->input_path, mappings[i].encrypted_name);
        snprintf(output_file_path, sizeof(output_file_path), "%s\\%s", args->output_path, mappings[i].original_name);
#else
        snprintf(input_file_path, sizeof(input_file_path), "%s/%s", args->input_path, mappings[i].encrypted_name);
        snprintf(output_file_path, sizeof(output_file_path), "%s/%s", args->output_path, mappings[i].original_name);
#endif
        
        // Создаем временные аргументы для обработки файла
        cli_args_t temp_args = *args;
        temp_args.input_path = input_file_path;
        temp_args.output_path = output_file_path;
        
        if (decrypt_single_file(&temp_args, key, key_hex) == 0) {
            success_count++;
        }
    }

    printf("Расшифровано %d из %d файлов\n", success_count, mapping_count);

    // Освобождаем память
    free_metadata(mappings, mapping_count, metadata_key_hex);
    free_file_list(files, file_count);

    return success_count == mapping_count ? 0 : 1;
}
