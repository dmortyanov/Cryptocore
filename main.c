#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/ecb.h"
#include "include/modes.h"
#include "include/file_io.h"

typedef struct {
    char* algorithm;
    char* mode;
    int encrypt;
    int decrypt;
    char* key_hex;
    char* iv_hex;
    char* input_file;
    char* output_file;
} cli_args_t;

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
    fprintf(stderr, "                         Если не указан при дешифровании, читается из начала файла\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Примечания:\n");
    fprintf(stderr, "  - Для режимов ecb: IV не используется\n");
    fprintf(stderr, "  - Для режимов cbc, cfb, ofb, ctr:\n");
    fprintf(stderr, "    * При шифровании: IV генерируется автоматически и добавляется в начало файла\n");
    fprintf(stderr, "    * При дешифровании: IV читается из начала файла или указывается через --iv\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Примеры:\n");
    fprintf(stderr, "  Шифрование (ECB):\n");
    fprintf(stderr, "    %s --algorithm aes --mode ecb --encrypt --key 000102030405060708090a0b0c0d0e0f --input plaintext.txt --output ciphertext.bin\n\n", program_name);
    fprintf(stderr, "  Шифрование (CBC, IV генерируется автоматически):\n");
    fprintf(stderr, "    %s --algorithm aes --mode cbc --encrypt --key 000102030405060708090a0b0c0d0e0f --input plaintext.txt --output ciphertext.bin\n\n", program_name);
    fprintf(stderr, "  Дешифрование (CBC, IV из файла):\n");
    fprintf(stderr, "    %s --algorithm aes --mode cbc --decrypt --key 000102030405060708090a0b0c0d0e0f --input ciphertext.bin --output decrypted.txt\n\n", program_name);
    fprintf(stderr, "  Дешифрование (CBC, явный IV):\n");
    fprintf(stderr, "    %s --algorithm aes --mode cbc --decrypt --key 000102030405060708090a0b0c0d0e0f --iv aabbccddeeff00112233445566778899 --input ciphertext.bin --output decrypted.txt\n", program_name);
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
            args->input_file = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Ошибка: --output требует аргумент\n");
                return -1;
            }
            args->output_file = argv[++i];
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
    if (!args->key_hex) {
        fprintf(stderr, "Ошибка: --key обязателен\n");
        return -1;
    }
    if (!args->input_file) {
        fprintf(stderr, "Ошибка: --input обязателен\n");
        return -1;
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
    size_t key_len = strlen(args->key_hex);
    if (key_len != 32) {
        fprintf(stderr, "Ошибка: Ключ AES-128 должен быть 32 шестнадцатеричных символа (16 байт)\n");
        fprintf(stderr, "       Предоставлена длина ключа: %zu символов\n", key_len);
        return -1;
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

    // Генерация имени выходного файла по умолчанию
    if (!args->output_file) {
        size_t len = strlen(args->input_file);
        if (args->encrypt) {
            args->output_file = (char*)malloc(len + 5);
            sprintf(args->output_file, "%s.enc", args->input_file);
        } else {
            args->output_file = (char*)malloc(len + 5);
            sprintf(args->output_file, "%s.dec", args->input_file);
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    cli_args_t args;
    unsigned char* key = NULL;
    unsigned char* iv = NULL;
    unsigned char* input_data = NULL;
    unsigned char* output_data = NULL;
    size_t key_size, iv_size, input_size, output_size;
    int result = 1;
    int needs_iv = 0;

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

    needs_iv = mode_requires_iv(args.mode);

    // Преобразование hex-ключа в байты
    key = hex_to_bytes(args.key_hex, &key_size);
    if (!key) {
        goto cleanup;
    }

    if (key_size != AES_128_KEY_SIZE) {
        fprintf(stderr, "Ошибка: Ключ должен быть ровно %d байт для AES-128\n", AES_128_KEY_SIZE);
        goto cleanup;
    }

    // Обработка IV
    if (needs_iv) {
        if (args.encrypt) {
            // Генерация случайного IV для шифрования
            iv = generate_iv();
            if (!iv) {
                goto cleanup;
            }
            printf("Сгенерирован IV: ");
            for (int i = 0; i < AES_BLOCK_SIZE; i++) {
                printf("%02x", iv[i]);
            }
            printf("\n");
        } else {
            // Для дешифрования
            if (args.iv_hex) {
                // IV предоставлен через командную строку
                iv = hex_to_bytes(args.iv_hex, &iv_size);
                if (!iv || iv_size != AES_BLOCK_SIZE) {
                    fprintf(stderr, "Ошибка: Неверный IV\n");
                    goto cleanup;
                }
            }
            // Если IV не предоставлен, он будет прочитан из файла позже
        }
    }

    // Чтение входного файла
    input_data = read_file(args.input_file, &input_size);
    if (!input_data) {
        goto cleanup;
    }

    // Для дешифрования с IV: если IV не был предоставлен, читаем его из файла
    if (needs_iv && args.decrypt && !iv) {
        if (input_size < AES_BLOCK_SIZE) {
            fprintf(stderr, "Ошибка: Файл слишком мал, чтобы содержать IV (минимум %d байт)\n", AES_BLOCK_SIZE);
            goto cleanup;
        }
        
        // Извлекаем IV из первых 16 байт
        iv = (unsigned char*)malloc(AES_BLOCK_SIZE);
        if (!iv) {
            fprintf(stderr, "Ошибка: Не удалось выделить память для IV\n");
            goto cleanup;
        }
        memcpy(iv, input_data, AES_BLOCK_SIZE);
        
        // Сдвигаем данные (убираем IV из входных данных)
        unsigned char* temp = (unsigned char*)malloc(input_size - AES_BLOCK_SIZE);
        if (!temp) {
            fprintf(stderr, "Ошибка: Не удалось выделить память\n");
            goto cleanup;
        }
        memcpy(temp, input_data + AES_BLOCK_SIZE, input_size - AES_BLOCK_SIZE);
        free(input_data);
        input_data = temp;
        input_size -= AES_BLOCK_SIZE;
        
        printf("IV прочитан из файла: ");
        for (int i = 0; i < AES_BLOCK_SIZE; i++) {
            printf("%02x", iv[i]);
        }
        printf("\n");
    }

    // Выполнение шифрования или дешифрования
    if (args.encrypt) {
        printf("Шифрование '%s' -> '%s' (режим: %s)\n", args.input_file, args.output_file, args.mode);
        
        if (strcmp(args.mode, "ecb") == 0) {
            output_data = aes_ecb_encrypt(input_data, input_size, key, &output_size);
        } else if (strcmp(args.mode, "cbc") == 0) {
            output_data = aes_cbc_encrypt(input_data, input_size, key, iv, &output_size);
        } else if (strcmp(args.mode, "cfb") == 0) {
            output_data = aes_cfb_encrypt(input_data, input_size, key, iv, &output_size);
        } else if (strcmp(args.mode, "ofb") == 0) {
            output_data = aes_ofb_encrypt(input_data, input_size, key, iv, &output_size);
        } else if (strcmp(args.mode, "ctr") == 0) {
            output_data = aes_ctr_encrypt(input_data, input_size, key, iv, &output_size);
        }
    } else {
        printf("Дешифрование '%s' -> '%s' (режим: %s)\n", args.input_file, args.output_file, args.mode);
        
        if (strcmp(args.mode, "ecb") == 0) {
            output_data = aes_ecb_decrypt(input_data, input_size, key, &output_size);
        } else if (strcmp(args.mode, "cbc") == 0) {
            output_data = aes_cbc_decrypt(input_data, input_size, key, iv, &output_size);
        } else if (strcmp(args.mode, "cfb") == 0) {
            output_data = aes_cfb_decrypt(input_data, input_size, key, iv, &output_size);
        } else if (strcmp(args.mode, "ofb") == 0) {
            output_data = aes_ofb_decrypt(input_data, input_size, key, iv, &output_size);
        } else if (strcmp(args.mode, "ctr") == 0) {
            output_data = aes_ctr_decrypt(input_data, input_size, key, iv, &output_size);
        }
    }

    if (!output_data) {
        goto cleanup;
    }

    // Для шифрования с IV: добавляем IV в начало файла
    if (needs_iv && args.encrypt) {
        unsigned char* final_output = (unsigned char*)malloc(AES_BLOCK_SIZE + output_size);
        if (!final_output) {
            fprintf(stderr, "Ошибка: Не удалось выделить память\n");
            goto cleanup;
        }
        
        memcpy(final_output, iv, AES_BLOCK_SIZE);
        memcpy(final_output + AES_BLOCK_SIZE, output_data, output_size);
        
        free(output_data);
        output_data = final_output;
        output_size += AES_BLOCK_SIZE;
    }

    // Запись выходного файла
    if (write_file(args.output_file, output_data, output_size) != 0) {
        goto cleanup;
    }

    printf("Успешно! Обработано %zu байт -> %zu байт\n", input_size, output_size);
    result = 0;

cleanup:
    if (key) free(key);
    if (iv) free(iv);
    if (input_data) free(input_data);
    if (output_data) free(output_data);

    return result;
}
