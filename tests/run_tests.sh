#!/bin/bash

# Тестовый скрипт CryptoCore
# Этот скрипт выполняет автоматизированные тесты для проверки корректности реализации

set -e  # Выход при ошибке

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # Без цвета

# Проверка существования бинарного файла cryptocore
if [ ! -f "../cryptocore" ]; then
    echo -e "${RED}Ошибка: бинарный файл cryptocore не найден. Пожалуйста, сначала выполните 'make'.${NC}"
    exit 1
fi

CRYPTOCORE="../cryptocore"
TEST_KEY="0123456789abcdef0123456789abcdef"

echo "=========================================="
echo "  Набор тестов CryptoCore"
echo "=========================================="
echo ""

# Тест 1: Базовый тест полного цикла (малый текстовый файл)
echo -e "${YELLOW}Тест 1: Базовый полный цикл (малый текст)${NC}"
echo "Привет, CryptoCore! Это тест." > test1.txt

$CRYPTOCORE --algorithm aes --mode ecb --encrypt \
    --key $TEST_KEY \
    --input test1.txt \
    --output test1.enc

$CRYPTOCORE --algorithm aes --mode ecb --decrypt \
    --key $TEST_KEY \
    --input test1.enc \
    --output test1.dec

if diff test1.txt test1.dec > /dev/null; then
    echo -e "${GREEN}✓ Тест 1 ПРОЙДЕН${NC}"
else
    echo -e "${RED}✗ Тест 1 НЕ ПРОЙДЕН${NC}"
    exit 1
fi
echo ""

# Тест 2: Большой текстовый файл
echo -e "${YELLOW}Тест 2: Большой текстовый файл${NC}"
# Создание большого файла (~1KB)
for i in {1..50}; do
    echo "Строка $i: Быстрая коричневая лиса прыгает через ленивую собаку. Lorem ipsum dolor sit amet."
done > test2.txt

$CRYPTOCORE --algorithm aes --mode ecb --encrypt \
    --key $TEST_KEY \
    --input test2.txt \
    --output test2.enc

$CRYPTOCORE --algorithm aes --mode ecb --decrypt \
    --key $TEST_KEY \
    --input test2.enc \
    --output test2.dec

if diff test2.txt test2.dec > /dev/null; then
    echo -e "${GREEN}✓ Тест 2 ПРОЙДЕН${NC}"
else
    echo -e "${RED}✗ Тест 2 НЕ ПРОЙДЕН${NC}"
    exit 1
fi
echo ""

# Тест 3: Бинарный файл
echo -e "${YELLOW}Тест 3: Бинарный файл${NC}"
# Создание бинарного файла
dd if=/dev/urandom of=test3.bin bs=1024 count=1 2>/dev/null

$CRYPTOCORE --algorithm aes --mode ecb --encrypt \
    --key $TEST_KEY \
    --input test3.bin \
    --output test3.enc

$CRYPTOCORE --algorithm aes --mode ecb --decrypt \
    --key $TEST_KEY \
    --input test3.enc \
    --output test3.dec

if diff test3.bin test3.dec > /dev/null; then
    echo -e "${GREEN}✓ Тест 3 ПРОЙДЕН${NC}"
else
    echo -e "${RED}✗ Тест 3 НЕ ПРОЙДЕН${NC}"
    exit 1
fi
echo ""

# Тест 4: Пустой файл
echo -e "${YELLOW}Тест 4: Пустой файл${NC}"
touch test4.txt

$CRYPTOCORE --algorithm aes --mode ecb --encrypt \
    --key $TEST_KEY \
    --input test4.txt \
    --output test4.enc

$CRYPTOCORE --algorithm aes --mode ecb --decrypt \
    --key $TEST_KEY \
    --input test4.enc \
    --output test4.dec

if diff test4.txt test4.dec > /dev/null; then
    echo -e "${GREEN}✓ Тест 4 ПРОЙДЕН${NC}"
else
    echo -e "${RED}✗ Тест 4 НЕ ПРОЙДЕН${NC}"
    exit 1
fi
echo ""

# Тест 5: Файл с точным размером блока (16 байт)
echo -e "${YELLOW}Тест 5: Файл с точным размером блока${NC}"
echo -n "1234567890abcdef" > test5.txt  # Ровно 16 байт

$CRYPTOCORE --algorithm aes --mode ecb --encrypt \
    --key $TEST_KEY \
    --input test5.txt \
    --output test5.enc

$CRYPTOCORE --algorithm aes --mode ecb --decrypt \
    --key $TEST_KEY \
    --input test5.enc \
    --output test5.dec

if diff test5.txt test5.dec > /dev/null; then
    echo -e "${GREEN}✓ Тест 5 ПРОЙДЕН${NC}"
else
    echo -e "${RED}✗ Тест 5 НЕ ПРОЙДЕН${NC}"
    exit 1
fi
echo ""

# Тест 6: Разные ключи дают разный шифртекст
echo -e "${YELLOW}Тест 6: Разные ключи дают разный шифртекст${NC}"
echo "Тестовое сообщение для сравнения ключей" > test6.txt

$CRYPTOCORE --algorithm aes --mode ecb --encrypt \
    --key 0123456789abcdef0123456789abcdef \
    --input test6.txt \
    --output test6_key1.enc

$CRYPTOCORE --algorithm aes --mode ecb --encrypt \
    --key fedcba9876543210fedcba9876543210 \
    --input test6.txt \
    --output test6_key2.enc

if ! diff test6_key1.enc test6_key2.enc > /dev/null; then
    echo -e "${GREEN}✓ Тест 6 ПРОЙДЕН (разные ключи дают разный вывод)${NC}"
else
    echo -e "${RED}✗ Тест 6 НЕ ПРОЙДЕН (разные ключи дали одинаковый вывод!)${NC}"
    exit 1
fi
echo ""

# Тест 7: Имя выходного файла по умолчанию
echo -e "${YELLOW}Тест 7: Имя выходного файла по умолчанию${NC}"
echo "Тестирование имени выходного файла по умолчанию" > test7.txt

$CRYPTOCORE --algorithm aes --mode ecb --encrypt \
    --key $TEST_KEY \
    --input test7.txt
    # --output не указан

if [ -f "test7.txt.enc" ]; then
    echo -e "${GREEN}✓ Тест 7 ПРОЙДЕН (создан файл .enc по умолчанию)${NC}"
    
    $CRYPTOCORE --algorithm aes --mode ecb --decrypt \
        --key $TEST_KEY \
        --input test7.txt.enc
        # --output не указан
    
    if [ -f "test7.txt.enc.dec" ] && diff test7.txt test7.txt.enc.dec > /dev/null; then
        echo -e "${GREEN}✓ Тест 7 ПРОЙДЕН (создан файл .dec по умолчанию и содержимое совпадает)${NC}"
    else
        echo -e "${RED}✗ Тест 7 НЕ ПРОЙДЕН (поведение .dec по умолчанию)${NC}"
        exit 1
    fi
else
    echo -e "${RED}✗ Тест 7 НЕ ПРОЙДЕН (файл .enc по умолчанию не создан)${NC}"
    exit 1
fi
echo ""

# Очистка
echo -e "${YELLOW}Очистка тестовых файлов...${NC}"
rm -f test*.txt test*.enc test*.dec test*.bin test*_key*.enc
echo ""

# Итоговая сводка
echo "=========================================="
echo -e "${GREEN}  Все тесты ПРОЙДЕНЫ! ✓${NC}"
echo "=========================================="
echo ""
echo "CryptoCore работает корректно!"
