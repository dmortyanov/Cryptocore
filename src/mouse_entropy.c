#include "../include/mouse_entropy.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ENTROPY_TARGET_BITS 128
#define SAFETY_TIMEOUT_MS 15000
#define SAMPLE_INTERVAL_MS 20

static void mix_sample(unsigned char* buffer, size_t len, POINT* point, DWORD timestamp) {
    unsigned char sample[sizeof(POINT) + sizeof(DWORD)];
    memcpy(sample, point, sizeof(POINT));
    memcpy(sample + sizeof(POINT), &timestamp, sizeof(DWORD));

    for (size_t i = 0; i < len; ++i) {
        buffer[i] ^= sample[i % sizeof(sample)];
        buffer[i] = (unsigned char)((buffer[i] << 5) | (buffer[i] >> 3));
        buffer[i] += (unsigned char)(timestamp >> (i % 24));
    }
}

static void derive_key(const unsigned char* buffer, size_t buf_len, unsigned char* key, size_t key_len) {
    unsigned char state[32];
    memset(state, 0, sizeof(state));

    for (size_t i = 0; i < buf_len; ++i) {
        state[i % sizeof(state)] ^= buffer[i];
        state[(i * 5 + 7) % sizeof(state)] = (unsigned char)((state[(i * 5 + 7) % sizeof(state)] << 3) | (state[(i * 5 + 7) % sizeof(state)] >> 5));
        state[i % sizeof(state)] += (unsigned char)(i * 31);
    }

    for (size_t i = 0; i < key_len; ++i) {
        key[i] = state[i % sizeof(state)] ^ state[(i * 7 + 11) % sizeof(state)];
        key[i] = (unsigned char)((key[i] << 1) | (key[i] >> 7));
        key[i] ^= (unsigned char)(i * 53);
    }
}

int generate_mouse_key(unsigned char* key_out, size_t key_len) {
#ifndef _WIN32
    (void)key_out;
    (void)key_len;
    fprintf(stderr, "Ошибка: генерация ключа по движению мыши поддерживается только в Windows.\n");
    return -1;
#else
    if (!key_out || key_len == 0) {
        return -1;
    }

    memset(key_out, 0, key_len);

    unsigned char entropy_buffer[256];
    memset(entropy_buffer, 0, sizeof(entropy_buffer));

    DWORD start = GetTickCount();
    DWORD collected = 0;
    size_t cursor = 0;

    printf("Пожалуйста, активно двигайте мышью...\n");

    while (collected < ENTROPY_TARGET_BITS) {
        POINT p;
        if (!GetCursorPos(&p)) {
            return -1;
        }

        DWORD now = GetTickCount();
        if (now - start > SAFETY_TIMEOUT_MS) {
            break;
        }

        mix_sample(entropy_buffer, sizeof(entropy_buffer), &p, now);

        cursor = (cursor + 1) % sizeof(entropy_buffer);
        entropy_buffer[cursor] ^= (unsigned char)(rand() & 0xFF);

        DWORD sleep_time = SAMPLE_INTERVAL_MS;
        Sleep(sleep_time);
        collected += 4;
    }

    if (collected < ENTROPY_TARGET_BITS) {
        fprintf(stderr, "Предупреждение: собрано недостаточно энтропии мыши.\n");
    }

    derive_key(entropy_buffer, sizeof(entropy_buffer), key_out, key_len);

    unsigned char system_entropy[32];
    if (BCryptGenRandom(NULL, system_entropy, sizeof(system_entropy), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) {
        for (size_t i = 0; i < key_len; ++i) {
            key_out[i] ^= system_entropy[i % sizeof(system_entropy)];
        }
    }

    memset(entropy_buffer, 0, sizeof(entropy_buffer));
    memset(system_entropy, 0, sizeof(system_entropy));

    return 0;
#endif
}

