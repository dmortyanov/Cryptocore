#!/bin/bash

# Тестовый скрипт совместимости CryptoCore с OpenSSL
# Этот скрипт проверяет, что CryptoCore может корректно работать с OpenSSL

set -e  # Выход при ошибке

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # Без цвета

# Проверка существования бинарного файла cryptocore
if [ ! -f "../cryptocore" ]; then
    echo -e "${RED}Ошибка: бинарный файл cryptocore не найден. Пожалуйста, сначала выполните 'make'.${NC}"
    exit 1
fi

# Проверка OpenSSL
if ! command -v openssl &> /dev/null; then
    echo -e "${RED}Ошибка: OpenSSL не найден. Пожалуйста, установите OpenSSL.${NC}"
    exit 1
fi

CRYPTOCORE="../cryptocore"
TEST_KEY="0123456789abcdef0123456789abcdef"
TEST_IV="aabbccddeeff00112233445566778899"

echo "=========================================="
echo "  Тесты совместимости CryptoCore с OpenSSL"
echo "=========================================="
echo ""

# Функция для тестирования режима
test_mode() {
    local mode=$1
    local mode_upper=$(echo $mode | tr '[:lower:]' '[:upper:]')
    
    echo -e "${BLUE}========== Тестирование режима $mode_upper ==========${NC}"
    
    # Создание тестового файла
    echo "Привет, CryptoCore! Тестирование совместимости с OpenSSL в режиме $mode_upper." > test_${mode}.txt
    
    # Тест 1: CryptoCore -> OpenSSL
    echo -e "${YELLOW}Тест 1: Шифрование в CryptoCore, дешифрование в OpenSSL${NC}"
    
    # Шифрование в CryptoCore
    $CRYPTOCORE --algorithm aes --mode $mode --encrypt \
        --key $TEST_KEY \
        --input test_${mode}.txt \
        --output test_${mode}_crypto.enc > /dev/null 2>&1
    
    if [ "$mode" != "ecb" ]; then
        # Извлечение IV из файла (первые 16 байт)
        dd if=test_${mode}_crypto.enc of=test_${mode}_iv.bin bs=16 count=1 2>/dev/null
        EXTRACTED_IV=$(xxd -p test_${mode}_iv.bin | tr -d '\n')
        
        # Извлечение шифртекста (все, кроме первых 16 байт)
        dd if=test_${mode}_crypto.enc of=test_${mode}_cipher.bin bs=16 skip=1 2>/dev/null
        
        # Дешифрование в OpenSSL
        openssl enc -aes-128-$mode -d \
            -K $TEST_KEY \
            -iv $EXTRACTED_IV \
            -in test_${mode}_cipher.bin \
            -out test_${mode}_openssl_dec.txt 2>/dev/null
    else
        # Для ECB IV не используется
        openssl enc -aes-128-$mode -d \
            -K $TEST_KEY \
            -in test_${mode}_crypto.enc \
            -out test_${mode}_openssl_dec.txt 2>/dev/null
    fi
    
    if diff test_${mode}.txt test_${mode}_openssl_dec.txt > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Тест 1 ПРОЙДЕН (CryptoCore -> OpenSSL)${NC}"
    else
        echo -e "${RED}✗ Тест 1 НЕ ПРОЙДЕН${NC}"
        return 1
    fi
    
    # Тест 2: OpenSSL -> CryptoCore
    echo -e "${YELLOW}Тест 2: Шифрование в OpenSSL, дешифрование в CryptoCore${NC}"
    
    if [ "$mode" != "ecb" ]; then
        # Шифрование в OpenSSL с IV
        openssl enc -aes-128-$mode \
            -K $TEST_KEY \
            -iv $TEST_IV \
            -in test_${mode}.txt \
            -out test_${mode}_openssl.enc 2>/dev/null
        
        # Дешифрование в CryptoCore с явным IV
        $CRYPTOCORE --algorithm aes --mode $mode --decrypt \
            --key $TEST_KEY \
            --iv $TEST_IV \
            --input test_${mode}_openssl.enc \
            --output test_${mode}_crypto_dec.txt > /dev/null 2>&1
    else
        # Для ECB
        openssl enc -aes-128-$mode \
            -K $TEST_KEY \
            -in test_${mode}.txt \
            -out test_${mode}_openssl.enc 2>/dev/null
        
        $CRYPTOCORE --algorithm aes --mode $mode --decrypt \
            --key $TEST_KEY \
            --input test_${mode}_openssl.enc \
            --output test_${mode}_crypto_dec.txt > /dev/null 2>&1
    fi
    
    if diff test_${mode}.txt test_${mode}_crypto_dec.txt > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Тест 2 ПРОЙДЕН (OpenSSL -> CryptoCore)${NC}"
    else
        echo -e "${RED}✗ Тест 2 НЕ ПРОЙДЕН${NC}"
        return 1
    fi
    
    echo -e "${GREEN}✓ Режим $mode_upper: ВСЕ ТЕСТЫ ПРОЙДЕНЫ${NC}"
    echo ""
}

# Тестирование всех режимов
MODES="cbc cfb ofb ctr ecb"
FAILED=0

for mode in $MODES; do
    if ! test_mode $mode; then
        FAILED=1
    fi
done

# Очистка
echo -e "${YELLOW}Очистка тестовых файлов...${NC}"
rm -f test_*.txt test_*.enc test_*.bin test_*_dec.txt
echo ""

# Итоговая сводка
if [ $FAILED -eq 0 ]; then
    echo "=========================================="
    echo -e "${GREEN}  ВСЕ ТЕСТЫ СОВМЕСТИМОСТИ ПРОЙДЕНЫ! ✓${NC}"
    echo "=========================================="
    echo ""
    echo "CryptoCore полностью совместим с OpenSSL!"
    exit 0
else
    echo "=========================================="
    echo -e "${RED}  НЕКОТОРЫЕ ТЕСТЫ НЕ ПРОШЛИ ✗${NC}"
    echo "=========================================="
    exit 1
fi

