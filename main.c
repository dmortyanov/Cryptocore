#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <stdarg.h>
#include <time.h>
#include <openssl/aes.h>
#include <sys/stat.h>
#include <dirent.h>
#include "include/ecb.h"
#include "include/modes.h"
#include "include/file_io.h"
#include "include/mouse_entropy.h"
#include "include/csprng.h"
#include "include/hash.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <io.h>
#include <direct.h>
#define stat _stat
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#else
#include <sys/stat.h>
#include <dirent.h>
#include <sys/resource.h>
#endif

typedef struct {
    char* algorithm;
    char* mode;
    int encrypt;
    int decrypt;
    int dgst;              // Hash mode flag
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

static void print_progress_bar(int current, int total) {
    if (total <= 0) return;
    const int bar_width = 30;
    double ratio = (double)current / (double)total;
    int pos = (int)(ratio * bar_width);
    printf("[");
    for (int i = 0; i < bar_width; i++) {
        if (i < pos) putchar('#');
        else if (i == pos) putchar('>');
        else putchar('.');
    }
    printf("] %d/%d\r", current, total);
    fflush(stdout);
}

static double get_memory_used_mb(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
    }
    return 0.0;
#else
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        return (double)ru.ru_maxrss / 1024.0; // KB -> MB on Linux usually in KB
    }
    return 0.0;
#endif
}

static void current_timestamp(char* buf, size_t sz) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(buf, sz, "%Y-%m-%d %H:%M:%S", t);
}

/* Forward declarations for logging helpers */
static void log_info(const char* fmt, ...);
static void log_error(const char* fmt, ...);

/* 64-bit file size helper */
static unsigned long long get_file_size64_path(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0ULL;
    
#if defined(_WIN32) && defined(__USE_MINGW_ANSI_STDIO)
    // MinGW/MSYS2: use _fseeki64 and _ftelli64
    if (_fseeki64(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0ULL;
    }
    long long size = _ftelli64(f);
#elif defined(_WIN32)
    // Native Windows
    if (_fseeki64(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0ULL;
    }
    long long size = _ftelli64(f);
#else
    // POSIX: use fseeko and ftello
    if (fseeko(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0ULL;
    }
    off_t size = ftello(f);
#endif
    
    fclose(f);
    if (size < 0) return 0ULL;
    return (unsigned long long)size;
}

/* Helpers for CTR streaming */
static int calc_percent(unsigned long long processed, unsigned long long total) {
    if (total == 0ULL) return 0;
    double p = ((double)processed * 100.0) / (double)total;
    if (p < 0.0) p = 0.0;
    if (p > 100.0) p = 100.0;
    return (int)(p + 0.5);
}
static void increment_counter_be(unsigned char* counter, unsigned long long blocks) {
    for (int i = 0; i < 16; i++) {
        unsigned int idx = 15 - i;
        unsigned int add = (unsigned int)(blocks & 0xFF);
        unsigned int sum = counter[idx] + add;
        counter[idx] = (unsigned char)(sum & 0xFF);
        blocks >>= 8;
        if ((sum >> 8) == 0 && blocks == 0) break;
    }
}

static int stream_encrypt_ctr_file(const char* in_path, const char* out_path, unsigned char* key, unsigned char* iv, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024; // 4 MB
    FILE* in = fopen(in_path, "rb");
    if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb");
    if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    if (fwrite(iv, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: failed to write IV to '%s'", out_path); fclose(in); fclose(out); return 1; }

    unsigned char* in_buf = (unsigned char*)malloc(CHUNK);
    unsigned char* out_buf = (unsigned char*)malloc(CHUNK);
    if (!in_buf || !out_buf) { log_error("Error: failed to allocate buffers"); if (in_buf) free(in_buf); if (out_buf) free(out_buf); fclose(in); fclose(out); return 1; }

    unsigned long long processed = 0ULL;
    unsigned long long total = get_file_size64_path(in_path);
    if (total == 0ULL) { log_error("Error: empty or unreadable file '%s'", in_path); free(in_buf); free(out_buf); fclose(in); fclose(out); return 1; }
    while (1) {
        size_t n = fread(in_buf, 1, CHUNK, in);
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(in_buf); free(out_buf); fclose(in); fclose(out); return 1; } break; }
        size_t out_size = 0;
        unsigned char iv_local[AES_BLOCK_SIZE];
        memcpy(iv_local, iv, AES_BLOCK_SIZE);
        unsigned char* enc = aes_ctr_encrypt(in_buf, n, key, iv_local, &out_size);
        if (!enc || out_size != n) { log_error("Error: CTR chunk encrypt failed"); free(in_buf); free(out_buf); fclose(in); fclose(out); if (enc) free(enc); return 1; }
        if (fwrite(enc, 1, out_size, out) != out_size) { log_error("Error: failed to write output chunk"); free(in_buf); free(out_buf); fclose(in); fclose(out); free(enc); return 1; }
        free(enc);
        unsigned long long blocks = (n + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;
        increment_counter_be(iv, blocks);
        processed += (unsigned long long)n;
        int percent = calc_percent(processed, total);
        printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); fflush(stdout);
    }
    printf("\n");
    if (processed == 0ULL) { log_error("Error: no data was processed"); free(in_buf); free(out_buf); fclose(in); fclose(out); return 1; }
    if (out_total) *out_total = (size_t)(processed + AES_BLOCK_SIZE);
    free(in_buf); free(out_buf); fclose(in); fclose(out); return 0;
}

static int stream_decrypt_ctr_file(const char* in_path, const char* out_path, unsigned char* key, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024; // 4 MB
    FILE* in = fopen(in_path, "rb");
    if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb");
    if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    unsigned char iv[AES_BLOCK_SIZE];
    if (fread(iv, 1, AES_BLOCK_SIZE, in) != AES_BLOCK_SIZE) { log_error("Error: input too small to contain IV"); fclose(in); fclose(out); return 1; }

    unsigned char* in_buf = (unsigned char*)malloc(CHUNK);
    if (!in_buf) { log_error("Error: failed to allocate buffer"); fclose(in); fclose(out); return 1; }
    unsigned long long processed = 0ULL;
    unsigned long long total = get_file_size64_path(in_path);
    if (total < AES_BLOCK_SIZE) { log_error("Error: file too small (no IV)"); free(in_buf); fclose(in); fclose(out); return 1; }
    total -= AES_BLOCK_SIZE;
    fseek(in, AES_BLOCK_SIZE, SEEK_SET);
    while (1) {
        size_t n = fread(in_buf, 1, CHUNK, in);
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(in_buf); fclose(in); fclose(out); return 1; } break; }
        size_t out_size = 0;
        unsigned char iv_local[AES_BLOCK_SIZE]; memcpy(iv_local, iv, AES_BLOCK_SIZE);
        unsigned char* dec = aes_ctr_decrypt(in_buf, n, key, iv_local, &out_size);
        if (!dec || out_size != n) { log_error("Error: CTR chunk decrypt failed"); free(in_buf); fclose(in); fclose(out); if (dec) free(dec); return 1; }
        if (fwrite(dec, 1, out_size, out) != out_size) { log_error("Error: failed to write output chunk"); free(in_buf); fclose(in); fclose(out); free(dec); return 1; }
        free(dec);
        unsigned long long blocks = (n + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;
        increment_counter_be(iv, blocks);
        processed += (unsigned long long)n;
        int percent = calc_percent(processed, total);
        printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); fflush(stdout);
    }
    printf("\n");
    if (processed == 0ULL && total > 0ULL) { log_error("Error: no data was processed"); free(in_buf); fclose(in); fclose(out); return 1; }
    if (out_total) *out_total = (size_t)processed;
    free(in_buf); fclose(in); fclose(out); return 0;
}

/* Streaming for ECB (PKCS#7) */
static int stream_encrypt_ecb_file(const char* in_path, const char* out_path, const unsigned char* key, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024; // 4 MB
    FILE* in = fopen(in_path, "rb"); if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb"); if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    AES_KEY aes_key; if (AES_set_encrypt_key(key, 128, &aes_key) < 0) { log_error("Error: AES_set_encrypt_key failed"); fclose(in); fclose(out); return 1; }
    unsigned char* buf = (unsigned char*)malloc(CHUNK + AES_BLOCK_SIZE);
    unsigned char out_block[AES_BLOCK_SIZE];
    size_t carry_len = 0; unsigned char carry[AES_BLOCK_SIZE];
    unsigned long long processed = 0ULL; unsigned long long total = get_file_size64_path(in_path);
    if (total == 0ULL) { log_error("Error: empty or unreadable file '%s'", in_path); free(buf); fclose(in); fclose(out); return 1; }
    while (1) {
        size_t n = fread(buf, 1, CHUNK, in);
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(buf); fclose(in); fclose(out); return 1; } break; }
        size_t offset = 0;
        if (carry_len > 0) {
            size_t need = AES_BLOCK_SIZE - carry_len;
            size_t to_copy = (n < need) ? n : need;
            memcpy(carry + carry_len, buf, to_copy);
            carry_len += to_copy; offset += to_copy;
            if (carry_len == AES_BLOCK_SIZE) {
                AES_encrypt(carry, out_block, &aes_key);
                if (fwrite(out_block, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: failed to write output block"); fclose(in); fclose(out); free(buf); return 1; }
                carry_len = 0;
            }
        }
        while (offset + AES_BLOCK_SIZE <= n) {
            AES_encrypt(buf + offset, out_block, &aes_key);
            if (fwrite(out_block, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: failed to write output block"); fclose(in); fclose(out); free(buf); return 1; }
            offset += AES_BLOCK_SIZE;
        }
        if (offset < n) { carry_len = n - offset; memcpy(carry, buf + offset, carry_len); }
        processed += (unsigned long long)n;
        int percent = calc_percent(processed, total); printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); fflush(stdout);
    }
    printf("\n");
    if (processed == 0ULL) { log_error("Error: no data was processed"); free(buf); fclose(in); fclose(out); return 1; }
    // finalize padding
    unsigned char pad = (unsigned char)(AES_BLOCK_SIZE - carry_len);
    for (size_t i = carry_len; i < AES_BLOCK_SIZE; i++) carry[i] = pad;
    AES_encrypt(carry, out_block, &aes_key);
    fwrite(out_block, 1, AES_BLOCK_SIZE, out);
    if (out_total) { /* processed bytes rounded up to block + one pad block */ *out_total = (size_t)(((processed / AES_BLOCK_SIZE) * AES_BLOCK_SIZE) + AES_BLOCK_SIZE); }
    free(buf); fclose(in); fclose(out); return 0;
}

static int stream_decrypt_ecb_file(const char* in_path, const char* out_path, const unsigned char* key, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024; // 4 MB
    FILE* in = fopen(in_path, "rb"); if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb"); if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    AES_KEY aes_key; if (AES_set_decrypt_key(key, 128, &aes_key) < 0) { log_error("Error: AES_set_decrypt_key failed"); fclose(in); fclose(out); return 1; }
    unsigned char* buf = (unsigned char*)malloc(CHUNK);
    unsigned char block[AES_BLOCK_SIZE], prev_block[AES_BLOCK_SIZE]; int have_prev = 0;
    unsigned long long processed = 0ULL; unsigned long long total = get_file_size64_path(in_path);
    if (total == 0ULL) { log_error("Error: empty or unreadable file '%s'", in_path); free(buf); fclose(in); fclose(out); return 1; }
    while (1) {
        size_t n = fread(buf, 1, CHUNK, in);
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(buf); fclose(in); fclose(out); return 1; } break; }
        size_t offset = 0;
        // ensure n is multiple of block for all but possibly last chunk; keep one block in prev
        while (offset + AES_BLOCK_SIZE <= n) {
            memcpy(block, buf + offset, AES_BLOCK_SIZE);
            offset += AES_BLOCK_SIZE;
            if (have_prev) {
                unsigned char plain[AES_BLOCK_SIZE];
                AES_decrypt(prev_block, plain, &aes_key);
                if (fwrite(plain, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: failed to write output block"); free(buf); fclose(in); fclose(out); return 1; }
            }
            memcpy(prev_block, block, AES_BLOCK_SIZE); have_prev = 1;
        }
        processed += n;
        int percent = calc_percent(processed, total); printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); fflush(stdout);
    }
    printf("\n");
    if (processed == 0ULL) { log_error("Error: no data was processed"); free(buf); fclose(in); fclose(out); return 1; }
    if (!have_prev) { log_error("Error: input too small"); free(buf); fclose(in); fclose(out); return 1; }
    // finalize: decrypt last block and remove padding
    unsigned char last_plain[AES_BLOCK_SIZE];
    AES_decrypt(prev_block, last_plain, &aes_key);
    unsigned char pad = last_plain[AES_BLOCK_SIZE - 1];
    if (pad == 0 || pad > AES_BLOCK_SIZE) { log_error("Error: invalid padding"); free(buf); fclose(in); fclose(out); return 1; }
    for (int i = 0; i < pad; i++) if (last_plain[AES_BLOCK_SIZE - 1 - i] != pad) { log_error("Error: invalid padding"); free(buf); fclose(in); fclose(out); return 1; }
    if (fwrite(last_plain, 1, AES_BLOCK_SIZE - pad, out) != AES_BLOCK_SIZE - pad) { log_error("Error: failed to write last block"); free(buf); fclose(in); fclose(out); return 1; }
    if (out_total) { long out_size = ftell(out); if (out_size < 0) out_size = 0; *out_total = (size_t)out_size; }
    free(buf); fclose(in); fclose(out); return 0;
}

/* Streaming for CBC (PKCS#7, with IV header) */
static int stream_encrypt_cbc_file(const char* in_path, const char* out_path, const unsigned char* key, unsigned char* iv_in, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024;
    FILE* in = fopen(in_path, "rb"); if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb"); if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    AES_KEY aes_key; if (AES_set_encrypt_key(key, 128, &aes_key) < 0) { log_error("Error: AES_set_encrypt_key failed"); fclose(in); fclose(out); return 1; }
    unsigned char iv[AES_BLOCK_SIZE]; memcpy(iv, iv_in, AES_BLOCK_SIZE);
    if (fwrite(iv, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: failed to write IV"); fclose(in); fclose(out); return 1; }
    unsigned char* buf = (unsigned char*)malloc(CHUNK + AES_BLOCK_SIZE);
    unsigned char carry[AES_BLOCK_SIZE]; size_t carry_len = 0;
    unsigned char block[AES_BLOCK_SIZE], cipher[AES_BLOCK_SIZE];
    unsigned long long processed = 0ULL; unsigned long long total = get_file_size64_path(in_path);
    if (total == 0ULL) { log_error("Error: empty or unreadable file '%s'", in_path); free(buf); fclose(in); fclose(out); return 1; }
    while (1) {
        size_t n = fread(buf, 1, CHUNK, in);
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(buf); fclose(in); fclose(out); return 1; } break; }
        size_t offset = 0;
        if (carry_len > 0) {
            size_t need = AES_BLOCK_SIZE - carry_len; size_t to_copy = (n < need) ? n : need;
            memcpy(carry + carry_len, buf, to_copy); carry_len += to_copy; offset += to_copy;
            if (carry_len == AES_BLOCK_SIZE) {
                xor_blocks(block, carry, iv, AES_BLOCK_SIZE); AES_encrypt(block, cipher, &aes_key); if (fwrite(cipher, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } memcpy(iv, cipher, AES_BLOCK_SIZE); carry_len = 0;
            }
        }
        while (offset + AES_BLOCK_SIZE <= n) {
            xor_blocks(block, buf + offset, iv, AES_BLOCK_SIZE); AES_encrypt(block, cipher, &aes_key); if (fwrite(cipher, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } memcpy(iv, cipher, AES_BLOCK_SIZE); offset += AES_BLOCK_SIZE;
        }
        if (offset < n) { carry_len = n - offset; memcpy(carry, buf + offset, carry_len); }
        processed += (unsigned long long)n; int percent = calc_percent(processed, total); printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); fflush(stdout);
    }
    printf("\n");
    if (processed == 0ULL) { log_error("Error: no data was processed"); free(buf); fclose(in); fclose(out); return 1; }
    // finalize pad
    unsigned char pad = (unsigned char)(AES_BLOCK_SIZE - carry_len);
    for (size_t i = carry_len; i < AES_BLOCK_SIZE; i++) carry[i] = pad;
    xor_blocks(block, carry, iv, AES_BLOCK_SIZE); AES_encrypt(block, cipher, &aes_key); fwrite(cipher, 1, AES_BLOCK_SIZE, out);
    if (out_total) *out_total = (size_t)(processed + AES_BLOCK_SIZE * 2); // IV + data + padding block
    free(buf); fclose(in); fclose(out); return 0;
}

static int stream_decrypt_cbc_file(const char* in_path, const char* out_path, const unsigned char* key, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024;
    FILE* in = fopen(in_path, "rb"); if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb"); if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    unsigned char iv[AES_BLOCK_SIZE]; if (fread(iv, 1, AES_BLOCK_SIZE, in) != AES_BLOCK_SIZE) { log_error("Error: input too small for IV"); fclose(in); fclose(out); return 1; }
    AES_KEY aes_key; if (AES_set_decrypt_key(key, 128, &aes_key) < 0) { log_error("Error: AES_set_decrypt_key failed"); fclose(in); fclose(out); return 1; }
    unsigned char* buf = (unsigned char*)malloc(CHUNK);
    unsigned char prev_cipher[AES_BLOCK_SIZE]; int have_prev = 0;
    unsigned long long processed = 0ULL; unsigned long long total = get_file_size64_path(in_path); 
    if (total < AES_BLOCK_SIZE) { log_error("Error: file too small (no IV)"); free(buf); fclose(in); fclose(out); return 1; }
    total -= AES_BLOCK_SIZE; fseek(in, AES_BLOCK_SIZE, SEEK_SET);
    while (1) {
        size_t n = fread(buf, 1, CHUNK, in);
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(buf); fclose(in); fclose(out); return 1; } break; }
        size_t offset = 0;
        while (offset + AES_BLOCK_SIZE <= n) {
            unsigned char cur_ciph[AES_BLOCK_SIZE]; memcpy(cur_ciph, buf + offset, AES_BLOCK_SIZE); offset += AES_BLOCK_SIZE;
            if (have_prev) {
                unsigned char temp[AES_BLOCK_SIZE], plain[AES_BLOCK_SIZE]; AES_decrypt(prev_cipher, temp, &aes_key); xor_blocks(plain, temp, iv, AES_BLOCK_SIZE); if (fwrite(plain, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } memcpy(iv, prev_cipher, AES_BLOCK_SIZE);
            }
            memcpy(prev_cipher, cur_ciph, AES_BLOCK_SIZE); have_prev = 1;
        }
        processed += (unsigned long long)n; int percent = calc_percent(processed, total); printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); fflush(stdout);
    }
    printf("\n");
    if (processed == 0ULL && total > 0ULL) { log_error("Error: no data was processed"); free(buf); fclose(in); fclose(out); return 1; }
    if (!have_prev) { log_error("Error: no ciphertext blocks"); free(buf); fclose(in); fclose(out); return 1; }
    unsigned char temp[AES_BLOCK_SIZE], last_plain[AES_BLOCK_SIZE]; AES_decrypt(prev_cipher, temp, &aes_key); xor_blocks(last_plain, temp, iv, AES_BLOCK_SIZE);
    unsigned char pad = last_plain[AES_BLOCK_SIZE - 1]; if (pad == 0 || pad > AES_BLOCK_SIZE) { log_error("Error: invalid padding"); free(buf); fclose(in); fclose(out); return 1; }
    for (int i = 0; i < pad; i++) if (last_plain[AES_BLOCK_SIZE - 1 - i] != pad) { log_error("Error: invalid padding"); free(buf); fclose(in); fclose(out); return 1; }
    if (fwrite(last_plain, 1, AES_BLOCK_SIZE - pad, out) != AES_BLOCK_SIZE - pad) { log_error("Error: write last"); free(buf); fclose(in); fclose(out); return 1; }
    if (out_total) *out_total = (size_t)processed; // approximation
    free(buf); fclose(in); fclose(out); return 0;
}

/* Streaming for CFB (full-block segment) */
static int stream_encrypt_cfb_file(const char* in_path, const char* out_path, const unsigned char* key, const unsigned char* iv_in, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024; 
    FILE* in = fopen(in_path, "rb"); if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb"); if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    if (fwrite(iv_in, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: failed to write IV"); fclose(in); fclose(out); return 1; }
    AES_KEY aes_key; if (AES_set_encrypt_key(key, 128, &aes_key) < 0) { log_error("Error: AES_set_encrypt_key failed"); fclose(in); fclose(out); return 1; }
    unsigned char* buf = (unsigned char*)malloc(CHUNK);
    unsigned char sr[AES_BLOCK_SIZE]; memcpy(sr, iv_in, AES_BLOCK_SIZE);
    unsigned long long processed = 0ULL; unsigned long long total = get_file_size64_path(in_path);
    if (total == 0ULL) { log_error("Error: empty or unreadable file '%s'", in_path); free(buf); fclose(in); fclose(out); return 1; }
    while (1) { 
        size_t n = fread(buf, 1, CHUNK, in); 
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(buf); fclose(in); fclose(out); return 1; } break; }
        size_t i = 0; 
        while (i + AES_BLOCK_SIZE <= n) { 
            unsigned char ks[AES_BLOCK_SIZE]; AES_encrypt(sr, ks, &aes_key); 
            unsigned char ct[AES_BLOCK_SIZE]; xor_blocks(ct, buf + i, ks, AES_BLOCK_SIZE); 
            if (fwrite(ct, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } 
            memcpy(sr, ct, AES_BLOCK_SIZE); i += AES_BLOCK_SIZE; 
        } 
        if (i < n) { 
            unsigned char ks[AES_BLOCK_SIZE]; AES_encrypt(sr, ks, &aes_key); 
            size_t rem = n - i; unsigned char ct[AES_BLOCK_SIZE]; 
            xor_blocks(ct, buf + i, ks, rem); 
            if (fwrite(ct, 1, rem, out) != rem) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } 
            memcpy(sr, ct, rem); 
        } 
        processed += (unsigned long long)n; 
        int percent = calc_percent(processed, total); 
        printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); 
        fflush(stdout);
    } 
    printf("\n"); 
    if (processed == 0ULL) { log_error("Error: no data was processed"); free(buf); fclose(in); fclose(out); return 1; }
    if (out_total) { *out_total = (size_t)(processed + AES_BLOCK_SIZE); } 
    free(buf); fclose(in); fclose(out); return 0; 
}

static int stream_decrypt_cfb_file(const char* in_path, const char* out_path, const unsigned char* key, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024; 
    FILE* in = fopen(in_path, "rb"); if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb"); if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    unsigned char sr[AES_BLOCK_SIZE]; if (fread(sr, 1, AES_BLOCK_SIZE, in) != AES_BLOCK_SIZE) { log_error("Error: input too small for IV"); fclose(in); fclose(out); return 1; }
    AES_KEY aes_key; if (AES_set_encrypt_key(key, 128, &aes_key) < 0) { log_error("Error: AES_set_encrypt_key failed"); fclose(in); fclose(out); return 1; }
    unsigned char* buf = (unsigned char*)malloc(CHUNK);
    unsigned long long processed = 0ULL; unsigned long long total = get_file_size64_path(in_path); 
    if (total < AES_BLOCK_SIZE) { log_error("Error: file too small (no IV)"); free(buf); fclose(in); fclose(out); return 1; }
    total -= AES_BLOCK_SIZE; fseek(in, AES_BLOCK_SIZE, SEEK_SET);
    while (1) { 
        size_t n = fread(buf, 1, CHUNK, in); 
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(buf); fclose(in); fclose(out); return 1; } break; }
        size_t i = 0; 
        while (i + AES_BLOCK_SIZE <= n) { 
            unsigned char ks[AES_BLOCK_SIZE]; AES_encrypt(sr, ks, &aes_key); 
            unsigned char pt[AES_BLOCK_SIZE]; xor_blocks(pt, buf + i, ks, AES_BLOCK_SIZE); 
            if (fwrite(pt, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } 
            memcpy(sr, buf + i, AES_BLOCK_SIZE); i += AES_BLOCK_SIZE; 
        } 
        if (i < n) { 
            unsigned char ks[AES_BLOCK_SIZE]; AES_encrypt(sr, ks, &aes_key); 
            size_t rem = n - i; unsigned char pt[AES_BLOCK_SIZE]; 
            xor_blocks(pt, buf + i, ks, rem); 
            if (fwrite(pt, 1, rem, out) != rem) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } 
            memcpy(sr, buf + i, rem); 
        } 
        processed += (unsigned long long)n; 
        int percent = calc_percent(processed, total); 
        printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); 
        fflush(stdout);
    } 
    printf("\n"); 
    if (processed == 0ULL && total > 0ULL) { log_error("Error: no data was processed"); free(buf); fclose(in); fclose(out); return 1; }
    if (out_total) { *out_total = (size_t)processed; } 
    free(buf); fclose(in); fclose(out); return 0; 
}

/* Streaming for OFB */
static int stream_encrypt_ofb_file(const char* in_path, const char* out_path, const unsigned char* key, const unsigned char* iv_in, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024; 
    FILE* in = fopen(in_path, "rb"); if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb"); if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    if (fwrite(iv_in, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: failed to write IV"); fclose(in); fclose(out); return 1; }
    AES_KEY aes_key; if (AES_set_encrypt_key(key, 128, &aes_key) < 0) { log_error("Error: AES_set_encrypt_key failed"); fclose(in); fclose(out); return 1; }
    unsigned char* buf = (unsigned char*)malloc(CHUNK);
    unsigned char feedback[AES_BLOCK_SIZE]; memcpy(feedback, iv_in, AES_BLOCK_SIZE);
    unsigned long long processed = 0ULL; unsigned long long total = get_file_size64_path(in_path);
    if (total == 0ULL) { log_error("Error: empty or unreadable file '%s'", in_path); free(buf); fclose(in); fclose(out); return 1; }
    while (1) { 
        size_t n = fread(buf, 1, CHUNK, in); 
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(buf); fclose(in); fclose(out); return 1; } break; }
        size_t i = 0; 
        while (i + AES_BLOCK_SIZE <= n) { 
            unsigned char ks[AES_BLOCK_SIZE]; AES_encrypt(feedback, ks, &aes_key); 
            unsigned char ct[AES_BLOCK_SIZE]; xor_blocks(ct, buf + i, ks, AES_BLOCK_SIZE); 
            if (fwrite(ct, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } 
            memcpy(feedback, ks, AES_BLOCK_SIZE); i += AES_BLOCK_SIZE; 
        } 
        if (i < n) { 
            unsigned char ks[AES_BLOCK_SIZE]; AES_encrypt(feedback, ks, &aes_key); 
            size_t rem = n - i; unsigned char ct[AES_BLOCK_SIZE]; 
            xor_blocks(ct, buf + i, ks, rem); 
            if (fwrite(ct, 1, rem, out) != rem) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } 
            memcpy(feedback, ks, AES_BLOCK_SIZE); 
        } 
        processed += (unsigned long long)n; 
        int percent = calc_percent(processed, total); 
        printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); 
        fflush(stdout);
    } 
    printf("\n"); 
    if (processed == 0ULL) { log_error("Error: no data was processed"); free(buf); fclose(in); fclose(out); return 1; }
    if (out_total) { *out_total = (size_t)(processed + AES_BLOCK_SIZE); } 
    free(buf); fclose(in); fclose(out); return 0; 
}

static int stream_decrypt_ofb_file(const char* in_path, const char* out_path, const unsigned char* key, size_t* out_total) {
    const size_t CHUNK = 4 * 1024 * 1024; 
    FILE* in = fopen(in_path, "rb"); if (!in) { log_error("Error: failed to open input file '%s'", in_path); return 1; }
    FILE* out = fopen(out_path, "wb"); if (!out) { log_error("Error: failed to open output file '%s'", out_path); fclose(in); return 1; }
    unsigned char feedback[AES_BLOCK_SIZE]; if (fread(feedback, 1, AES_BLOCK_SIZE, in) != AES_BLOCK_SIZE) { log_error("Error: input too small for IV"); fclose(in); fclose(out); return 1; }
    AES_KEY aes_key; if (AES_set_encrypt_key(key, 128, &aes_key) < 0) { log_error("Error: AES_set_encrypt_key failed"); fclose(in); fclose(out); return 1; }
    unsigned char* buf = (unsigned char*)malloc(CHUNK);
    unsigned long long processed = 0ULL; unsigned long long total = get_file_size64_path(in_path); 
    if (total < AES_BLOCK_SIZE) { log_error("Error: file too small (no IV)"); free(buf); fclose(in); fclose(out); return 1; }
    total -= AES_BLOCK_SIZE; fseek(in, AES_BLOCK_SIZE, SEEK_SET);
    while (1) { 
        size_t n = fread(buf, 1, CHUNK, in); 
        if (n == 0) { if (ferror(in)) { log_error("Error reading input file"); free(buf); fclose(in); fclose(out); return 1; } break; }
        size_t i = 0; 
        while (i + AES_BLOCK_SIZE <= n) { 
            unsigned char ks[AES_BLOCK_SIZE]; AES_encrypt(feedback, ks, &aes_key); 
            unsigned char pt[AES_BLOCK_SIZE]; xor_blocks(pt, buf + i, ks, AES_BLOCK_SIZE); 
            if (fwrite(pt, 1, AES_BLOCK_SIZE, out) != AES_BLOCK_SIZE) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } 
            memcpy(feedback, ks, AES_BLOCK_SIZE); i += AES_BLOCK_SIZE; 
        } 
        if (i < n) { 
            unsigned char ks[AES_BLOCK_SIZE]; AES_encrypt(feedback, ks, &aes_key); 
            size_t rem = n - i; unsigned char pt[AES_BLOCK_SIZE]; 
            xor_blocks(pt, buf + i, ks, rem); 
            if (fwrite(pt, 1, rem, out) != rem) { log_error("Error: write"); free(buf); fclose(in); fclose(out); return 1; } 
            memcpy(feedback, ks, AES_BLOCK_SIZE); 
        } 
        processed += (unsigned long long)n; 
        int percent = calc_percent(processed, total); 
        printf("\rProgress: %3d%%, Processed: %llu / %llu bytes", percent, processed, total); 
        fflush(stdout);
    } 
    printf("\n"); 
    if (processed == 0ULL && total > 0ULL) { log_error("Error: no data was processed"); free(buf); fclose(in); fclose(out); return 1; }
    if (out_total) { *out_total = (size_t)processed; } 
    free(buf); fclose(in); fclose(out); return 0; 
}

static void log_info(const char* fmt, ...) {
    char ts[32];
    current_timestamp(ts, sizeof(ts));
    double mem = get_memory_used_mb();
    printf("[%s] [Memory: %.2f MB] ", ts, mem);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

static void log_error(const char* fmt, ...) {
    char ts[32];
    current_timestamp(ts, sizeof(ts));
    double mem = get_memory_used_mb();
    fprintf(stderr, "[%s] [Memory: %.2f MB] ", ts, mem);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

/**
 * Вывод информации об использовании программы
 */
void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s [COMMAND] [OPTIONS]\n\n", program_name);
    
    fprintf(stderr, "COMMANDS:\n");
    fprintf(stderr, "  (default)              Encryption/decryption mode\n");
    fprintf(stderr, "  dgst                   Hash computation mode\n\n");
    
    fprintf(stderr, "=== ENCRYPTION/DECRYPTION MODE ===\n");
    fprintf(stderr, "Required options:\n");
    fprintf(stderr, "  --algorithm ALG        Encryption algorithm (supported: aes)\n");
    fprintf(stderr, "  --mode MODE            Mode (ecb, cbc, cfb, ofb, ctr)\n");
    fprintf(stderr, "  --encrypt              Perform encryption\n");
    fprintf(stderr, "  --decrypt              Perform decryption\n");
    fprintf(stderr, "  --key KEY              Encryption/Decryption key (hex string, 32 chars for AES-128)\n");
    fprintf(stderr, "  --input FILE           Path to input file or directory\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Optional:\n");
    fprintf(stderr, "  --output FILE          Path to output file or directory (default: <input>.enc or <input>.dec)\n");
    fprintf(stderr, "  --iv IV                Initialization Vector (decrypt only, hex string, 32 chars)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Notes:\n");
    fprintf(stderr, "  - On encryption a key can be generated automatically (mouse/CSPRNG)\n");
    fprintf(stderr, "  - On directory encryption the program will prompt for new names for each file\n");
    fprintf(stderr, "  - For ecb: IV is not used\n");
    fprintf(stderr, "  - For cbc, cfb, ofb, ctr:\n");
    fprintf(stderr, "    * Encryption: IV is generated automatically and prepended to the file\n");
    fprintf(stderr, "    * Decryption: IV is read from the beginning of the file or provided via --iv\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  Encrypt directory:\n");
    fprintf(stderr, "    %s --algorithm aes --mode cbc --encrypt --input ./files --output ./encryptfiles\n\n", program_name);
    fprintf(stderr, "  Decrypt directory:\n");
    fprintf(stderr, "    %s --algorithm aes --mode cbc --decrypt --input ./encryptfiles --output ./decryptfiles\n\n", program_name);
    
    fprintf(stderr, "=== HASH MODE (dgst command) ===\n");
    fprintf(stderr, "Required options:\n");
    fprintf(stderr, "  --algorithm ALG        Hash algorithm (sha256, sha3-256)\n");
    fprintf(stderr, "  --input FILE           Path to input file\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Optional:\n");
    fprintf(stderr, "  --output FILE          Write hash to file instead of stdout\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  Compute SHA-256 hash:\n");
    fprintf(stderr, "    %s dgst --algorithm sha256 --input document.pdf\n\n", program_name);
    fprintf(stderr, "  Compute SHA3-256 hash and save to file:\n");
    fprintf(stderr, "    %s dgst --algorithm sha3-256 --input backup.tar --output backup.sha3\n", program_name);
}

/**
 * Разбор аргументов командной строки
 */
int parse_args(int argc, char* argv[], cli_args_t* args) {
    // Инициализация аргументов
    memset(args, 0, sizeof(cli_args_t));

    // Check for dgst command
    if (argc > 1 && strcmp(argv[1], "dgst") == 0) {
        args->dgst = 1;
        // Skip "dgst" argument
        for (int i = 2; i < argc; i++) {
            argv[i-1] = argv[i];
        }
        argc--;
    }

    // Разбор аргументов
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--algorithm") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --algorithm requires an argument\n");
                return -1;
            }
            args->algorithm = argv[++i];
        } else if (strcmp(argv[i], "--mode") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --mode requires an argument\n");
                return -1;
            }
            args->mode = argv[++i];
        } else if (strcmp(argv[i], "--encrypt") == 0) {
            args->encrypt = 1;
        } else if (strcmp(argv[i], "--decrypt") == 0) {
            args->decrypt = 1;
        } else if (strcmp(argv[i], "--key") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --key requires an argument\n");
                return -1;
            }
            args->key_hex = argv[++i];
        } else if (strcmp(argv[i], "--iv") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --iv requires an argument\n");
                return -1;
            }
            args->iv_hex = argv[++i];
        } else if (strcmp(argv[i], "--input") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --input requires an argument\n");
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
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
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
        fprintf(stderr, "Error: --algorithm is required\n");
        return -1;
    }
    if (!args->mode) {
        fprintf(stderr, "Error: --mode is required\n");
        return -1;
    }
    if (args->encrypt) {
        args->use_mouse_key = 1;  // Автоматически используем генерацию ключа по мыши при шифровании
    }
    
    // Sprint 3: --key теперь опциональный для шифрования
    if (!args->key_hex && !args->use_mouse_key && !args->encrypt) {
        fprintf(stderr, "Error: --key is required for decryption\n");
        return -1;
    }
    if (!args->input_path) {
        fprintf(stderr, "Error: --input is required\n");
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
        fprintf(stderr, "Error: cannot use --encrypt and --decrypt together\n");
        return -1;
    }
    if (!args->encrypt && !args->decrypt) {
        fprintf(stderr, "Error: one of --encrypt or --decrypt must be specified\n");
        return -1;
    }

    if (args->use_mouse_key) {
#ifndef _WIN32
        fprintf(stderr, "Error: mouse-based key generation is available only on Windows build\n");
        return -1;
#else
        if (args->key_hex) {
            fprintf(stderr, "Warning: --key is ignored when automatic key generation is enabled\n");
            args->key_hex = NULL;
        }
#endif
    }
    
    // Проверка IV согласно Sprint 2 требованиям
    int needs_iv = mode_requires_iv(args->mode);
    
    if (needs_iv) {
        if (args->encrypt && args->iv_hex) {
            fprintf(stderr, "Warning: --iv is ignored during encryption (IV is generated automatically)\n");
            args->iv_hex = NULL;
        }
        // При дешифровании IV может быть не указан - будет читаться из файла
    }

    // Проверка алгоритма
    if (strcmp(args->algorithm, "aes") != 0) {
        fprintf(stderr, "Error: unsupported algorithm '%s'. Only 'aes' is supported.\n", args->algorithm);
        return -1;
    }

    // Проверка режима
    if (strcmp(args->mode, "ecb") != 0 && strcmp(args->mode, "cbc") != 0 &&
        strcmp(args->mode, "cfb") != 0 && strcmp(args->mode, "ofb") != 0 &&
        strcmp(args->mode, "ctr") != 0) {
        fprintf(stderr, "Error: unsupported mode '%s'. Supported: ecb, cbc, cfb, ofb, ctr.\n", args->mode);
        return -1;
    }

    // Проверка длины ключа
    if (args->key_hex) {
    size_t key_len = strlen(args->key_hex);
    if (key_len != 32) {
        fprintf(stderr, "Error: AES-128 key must be 32 hex chars (16 bytes)\n");
        fprintf(stderr, "       Provided key length: %zu chars\n", key_len);
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
            fprintf(stderr, "Error: IV must be 32 hex chars (16 bytes)\n");
            fprintf(stderr, "       Provided IV length: %zu chars\n", iv_len);
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
    printf("Enter new name for '%s': ", original_name);
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

/**
 * Handle dgst command for computing file hashes
 */
int handle_dgst_command(cli_args_t* args) {
    uint8_t hash[32];
    char hex_hash[65];
    
    // Validate arguments
    if (!args->algorithm) {
        fprintf(stderr, "Error: --algorithm is required for dgst command\n");
        return 1;
    }
    
    if (!args->input_path) {
        fprintf(stderr, "Error: --input is required for dgst command\n");
        return 1;
    }
    
    // Compute hash based on algorithm
    int hash_result = -1;
    if (strcmp(args->algorithm, "sha256") == 0) {
        hash_result = sha256_hash_file(args->input_path, hash);
    } else if (strcmp(args->algorithm, "sha3-256") == 0) {
        hash_result = sha3_256_hash_file(args->input_path, hash);
    } else {
        fprintf(stderr, "Error: Unsupported hash algorithm '%s'\n", args->algorithm);
        fprintf(stderr, "Supported: sha256, sha3-256\n");
        return 1;
    }
    
    // Check if hashing was successful
    if (hash_result != 0) {
        return 1;
    }
    
    hash_to_hex(hash, 32, hex_hash);
    
    // Output hash
    if (args->output_path) {
        // Write to file
        FILE* f = fopen(args->output_path, "w");
        if (!f) {
            fprintf(stderr, "Error: Failed to open output file '%s'\n", args->output_path);
            return 1;
        }
        fprintf(f, "%s  %s\n", hex_hash, args->input_path);
        fclose(f);
    } else {
        // Print to stdout
        printf("%s  %s\n", hex_hash, args->input_path);
    }

    return 0;
}

int main(int argc, char* argv[]) {
    /* Устанавливаем локаль/кодировку консоли на UTF-8 */
    setlocale(LC_ALL, "");
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
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

    // Handle dgst command separately
    if (args.dgst) {
        return handle_dgst_command(&args);
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
            log_error("Error: failed to allocate memory for key");
            goto cleanup;
        }
    log_info("Starting crypto utility session...");
    log_info("Generating encryption key (mouse entropy)...");
        if (generate_mouse_key(key, AES_128_KEY_SIZE) != 0) {
            log_error("Error: failed to generate mouse-based key");
            goto cleanup;
        }
        log_info("Key generated successfully!");
        
        // Преобразуем ключ в hex для сохранения
        key_hex = malloc(AES_128_KEY_SIZE * 2 + 1);
        for (int i = 0; i < AES_128_KEY_SIZE; i++) {
            sprintf(key_hex + i * 2, "%02x", key[i]);
        }
        printf("Key (hex): %s\n", key_hex);
#else
        log_error("Error: mouse-based key generation is Windows-only");
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
        log_error("Error: key must be exactly %d bytes for AES-128", AES_128_KEY_SIZE);
        goto cleanup;
    }

        // Проверка на слабые ключи
        if (is_weak_key(key, AES_128_KEY_SIZE)) {
            log_error("Warning: weak key detected. Use a cryptographically strong key.");
        }
    } else if (args.encrypt) {
        // Sprint 3: Генерация случайного ключа при шифровании
        key = (unsigned char*)malloc(AES_128_KEY_SIZE);
        if (!key) {
            log_error("Error: failed to allocate memory for key");
                goto cleanup;
            }
        
        if (generate_aes_key(key) != 0) {
            log_error("Error: failed to generate cryptographically secure key");
            goto cleanup;
        }
        
        // Преобразуем ключ в hex для вывода
        key_hex = malloc(AES_128_KEY_SIZE * 2 + 1);
        for (int i = 0; i < AES_128_KEY_SIZE; i++) {
            sprintf(key_hex + i * 2, "%02x", key[i]);
        }
        
        // Print generated key (Sprint 3 requirement)
        log_info("Generated random key: %s", key_hex);
        } else {
        // For decryption key is required
        log_error("Error: --key is required for decryption");
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

    // Потоковая обработка (4MB) для всех режимов
    if (strcmp(args->mode, "ctr") == 0) {
        int needs_iv_stream = 1;
        unsigned char* iv_local = NULL;
        if (needs_iv_stream) {
            iv_local = malloc(AES_BLOCK_SIZE);
            if (!iv_local) goto cleanup;
            if (generate_random_iv(iv_local) != 0) { log_error("Error: failed to generate cryptographically secure IV"); goto cleanup; }
        }
        log_info("Encrypt '%s' -> '%s' (mode: ctr, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb();
        clock_t t_start = clock();
        size_t out_total = 0;
        int sres = stream_encrypt_ctr_file(args->input_path, args->output_path, key, iv_local, &out_total);
        clock_t t_end = clock();
        double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC;
        double mem_after_mb = get_memory_used_mb();
        if (iv_local) free(iv_local);
        if (sres != 0) goto cleanup;
        double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
        log_info("Success! Processed -> %zu bytes", out_total);
        log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)",
                 elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0;
        goto cleanup;
    }
    if (strcmp(args->mode, "ecb") == 0) {
        log_info("Encrypt '%s' -> '%s' (mode: ecb, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb(); clock_t t_start = clock(); size_t out_total = 0;
        int sres = stream_encrypt_ecb_file(args->input_path, args->output_path, key, &out_total);
        clock_t t_end = clock(); double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC; double mem_after_mb = get_memory_used_mb();
        if (sres != 0) goto cleanup; double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
        log_info("Success! Processed -> %zu bytes", out_total);
        log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)", elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0; goto cleanup;
    }
    if (strcmp(args->mode, "cbc") == 0) {
        unsigned char iv_local[AES_BLOCK_SIZE]; if (generate_random_iv(iv_local) != 0) { log_error("Error: failed to generate cryptographically secure IV"); goto cleanup; }
        log_info("Encrypt '%s' -> '%s' (mode: cbc, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb(); clock_t t_start = clock(); size_t out_total = 0;
        int sres = stream_encrypt_cbc_file(args->input_path, args->output_path, key, iv_local, &out_total);
        clock_t t_end = clock(); double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC; double mem_after_mb = get_memory_used_mb();
        if (sres != 0) goto cleanup; double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
        log_info("Success! Processed -> %zu bytes", out_total);
        log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)", elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0; goto cleanup;
    }
    if (strcmp(args->mode, "cfb") == 0) {
        unsigned char iv_local[AES_BLOCK_SIZE]; if (generate_random_iv(iv_local) != 0) { log_error("Error: failed to generate cryptographically secure IV"); goto cleanup; }
        log_info("Encrypt '%s' -> '%s' (mode: cfb, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb(); clock_t t_start = clock(); size_t out_total = 0;
        int sres = stream_encrypt_cfb_file(args->input_path, args->output_path, key, iv_local, &out_total);
        clock_t t_end = clock(); double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC; double mem_after_mb = get_memory_used_mb();
        if (sres != 0) goto cleanup; double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
        log_info("Success! Processed -> %zu bytes", out_total);
        log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)", elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0; goto cleanup;
    }
    if (strcmp(args->mode, "ofb") == 0) {
        unsigned char iv_local[AES_BLOCK_SIZE]; if (generate_random_iv(iv_local) != 0) { log_error("Error: failed to generate cryptographically secure IV"); goto cleanup; }
        log_info("Encrypt '%s' -> '%s' (mode: ofb, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb(); clock_t t_start = clock(); size_t out_total = 0;
        int sres = stream_encrypt_ofb_file(args->input_path, args->output_path, key, iv_local, &out_total);
        clock_t t_end = clock(); double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC; double mem_after_mb = get_memory_used_mb();
        if (sres != 0) goto cleanup; double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
        log_info("Success! Processed -> %zu bytes", out_total);
        log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)", elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0; goto cleanup;
    }

    // Чтение входного файла
    double mem_before_mb = get_memory_used_mb();
    clock_t t_start = clock();
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
            fprintf(stderr, "Error: failed to generate cryptographically secure IV\n");
            goto cleanup;
        }
    }

    // Шифрование
    log_info("Encrypt '%s' -> '%s' (mode: %s)", args->input_path, args->output_path, args->mode);
    
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

    clock_t t_end = clock();
    double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC;
    double mem_after_mb = get_memory_used_mb();
    double mbps = (elapsed_sec > 0.0) ? ((double)output_size / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
    log_info("Success! Processed %zu bytes -> %zu bytes", input_size, output_size);
    log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (diff: %.2f MB)",
             elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
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
                fprintf(stderr, "Error: invalid IV\n");
                goto cleanup;
            }
        } else {
            // IV читается из начала файла
            if (input_size < AES_BLOCK_SIZE) {
                fprintf(stderr, "Error: file is too small to contain IV\n");
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

    // Потоковая обработка для всех режимов
    if (strcmp(args->mode, "ctr") == 0) {
    log_info("Decrypt '%s' -> '%s' (mode: ctr, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb();
        clock_t t_start = clock();
        size_t out_total = 0;
        int sres = stream_decrypt_ctr_file(args->input_path, args->output_path, key, &out_total);
        clock_t t_end = clock();
        double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC;
        double mem_after_mb = get_memory_used_mb();
        if (sres != 0) goto cleanup;
        double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
    log_info("Success! Processed -> %zu bytes", out_total);
    log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)",
                 elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0;
        goto cleanup;
    }
    if (strcmp(args->mode, "ecb") == 0) {
    log_info("Decrypt '%s' -> '%s' (mode: ecb, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb(); clock_t t_start = clock(); size_t out_total = 0;
        int sres = stream_decrypt_ecb_file(args->input_path, args->output_path, key, &out_total);
        clock_t t_end = clock(); double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC; double mem_after_mb = get_memory_used_mb();
        if (sres != 0) goto cleanup; double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
    log_info("Success! Processed -> %zu bytes", out_total);
    log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)", elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0; goto cleanup;
    }
    if (strcmp(args->mode, "cbc") == 0) {
    log_info("Decrypt '%s' -> '%s' (mode: cbc, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb(); clock_t t_start = clock(); size_t out_total = 0;
        int sres = stream_decrypt_cbc_file(args->input_path, args->output_path, key, &out_total);
        clock_t t_end = clock(); double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC; double mem_after_mb = get_memory_used_mb();
        if (sres != 0) goto cleanup; double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
    log_info("Success! Processed -> %zu bytes", out_total);
    log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)", elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0; goto cleanup;
    }
    if (strcmp(args->mode, "cfb") == 0) {
    log_info("Decrypt '%s' -> '%s' (mode: cfb, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb(); clock_t t_start = clock(); size_t out_total = 0;
        int sres = stream_decrypt_cfb_file(args->input_path, args->output_path, key, &out_total);
        clock_t t_end = clock(); double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC; double mem_after_mb = get_memory_used_mb();
        if (sres != 0) goto cleanup; double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
    log_info("Success! Processed -> %zu bytes", out_total);
    log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)", elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0; goto cleanup;
    }
    if (strcmp(args->mode, "ofb") == 0) {
    log_info("Decrypt '%s' -> '%s' (mode: ofb, streaming)", args->input_path, args->output_path);
        double mem_before_mb = get_memory_used_mb(); clock_t t_start = clock(); size_t out_total = 0;
        int sres = stream_decrypt_ofb_file(args->input_path, args->output_path, key, &out_total);
        clock_t t_end = clock(); double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC; double mem_after_mb = get_memory_used_mb();
        if (sres != 0) goto cleanup; double mbps = (elapsed_sec > 0.0) ? ((double)out_total / (1024.0 * 1024.0)) / elapsed_sec : 0.0;
    log_info("Success! Processed -> %zu bytes", out_total);
    log_info("Time: %.3f s, Speed: %.2f MB/s, Memory: %.2f MB -> %.2f MB (Δ %.2f MB)", elapsed_sec, mbps, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
        result = 0; goto cleanup;
    }

    // Дешифрование
    double mem_before_mb = get_memory_used_mb();
    clock_t t_start = clock();
    log_info("Decrypt '%s' -> '%s' (mode: %s)", args->input_path, args->output_path, args->mode);
    
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

    clock_t t_end = clock();
    double elapsed_sec = (double)(t_end - t_start) / CLOCKS_PER_SEC;
    double mem_after_mb = get_memory_used_mb();
    log_info("Success! Processed %zu bytes -> %zu bytes", input_size, output_size);
    log_info("Time: %.3f s, Memory: %.2f MB -> %.2f MB (diff %.2f MB)",
             elapsed_sec, mem_before_mb, mem_after_mb, mem_after_mb - mem_before_mb);
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
        fprintf(stderr, "Error: failed to list files in directory '%s'\n", args->input_path);
        return 1;
    }

    if (file_count == 0) {
        printf("Directory '%s' is empty\n", args->input_path);
        free_file_list(files, file_count);
        return 0;
    }

    // Создаем выходную директорию
    if (ensure_directory_exists(args->output_path) != 0) {
        fprintf(stderr, "Error: failed to create output directory '%s'\n", args->output_path);
        free_file_list(files, file_count);
        return 1;
    }

    filename_mapping_t* mappings = malloc(file_count * sizeof(filename_mapping_t));
    int success_count = 0;

    printf("Found %d files to encrypt\n", file_count);

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
            fprintf(stderr, "Error: failed to get new name for file '%s'\n", files[i]);
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
        print_progress_bar(i + 1, file_count);
        
        free(new_name);
    }

    // Прогресс переносом строки после бара
    printf("\n");

    // Записываем метаданные
    if (success_count > 0) {
        write_metadata_file(args->output_path, mappings, success_count, key_hex);
        printf("Encrypted %d of %d files\n", success_count, file_count);
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
        fprintf(stderr, "Error: failed to list files in directory '%s'\n", args->input_path);
        return 1;
    }

    // Читаем метаданные
    int mapping_count;
    char* metadata_key_hex;
    filename_mapping_t* mappings = read_metadata_file(args->input_path, &mapping_count, &metadata_key_hex);
    
    if (!mappings) {
        fprintf(stderr, "Error: failed to read metadata from directory '%s'\n", args->input_path);
        free_file_list(files, file_count);
        return 1;
    }

    // Проверяем ключ
    if (strcmp(key_hex, metadata_key_hex) != 0) {
        fprintf(stderr, "Error: provided key does not match the one used for encryption\n");
        free_metadata(mappings, mapping_count, metadata_key_hex);
        free_file_list(files, file_count);
        return 1;
    }

    // Создаем выходную директорию
    if (ensure_directory_exists(args->output_path) != 0) {
        fprintf(stderr, "Error: failed to create output directory '%s'\n", args->output_path);
        free_metadata(mappings, mapping_count, metadata_key_hex);
        free_file_list(files, file_count);
        return 1;
    }

    int success_count = 0;

    printf("Found %d encrypted files to decrypt\n", mapping_count);

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
        print_progress_bar(i + 1, mapping_count);
    }

    // Прогресс переносом строки после бара
    printf("\n");

    printf("Decrypted %d of %d files\n", success_count, mapping_count);

    // Освобождаем память
    free_metadata(mappings, mapping_count, metadata_key_hex);
    free_file_list(files, file_count);

    return success_count == mapping_count ? 0 : 1;
}
