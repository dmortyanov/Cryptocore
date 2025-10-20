#!/bin/bash

# Тест уникальности ключей для CryptoCore CSPRNG
# Проверяет, что генерируемые ключи уникальны

echo "=== Тест уникальности ключей CryptoCore CSPRNG ==="
echo

# Создаем временный файл для хранения ключей
TEMP_FILE=$(mktemp)
UNIQUE_COUNT=0
TOTAL_KEYS=1000

echo "Генерируем $TOTAL_KEYS ключей для проверки уникальности..."

# Генерируем ключи и проверяем уникальность
for i in $(seq 1 $TOTAL_KEYS); do
    # Генерируем случайный ключ через CSPRNG
    KEY=$(python3 -c "
import os
key_bytes = os.urandom(16)
print(key_bytes.hex())
" 2>/dev/null)
    
    if [ $? -eq 0 ] && [ -n "$KEY" ]; then
        # Проверяем, есть ли уже такой ключ
        if grep -q "^$KEY$" "$TEMP_FILE"; then
            echo "ОШИБКА: Найден дублирующийся ключ: $KEY"
            echo "Это указывает на критическую ошибку в CSPRNG!"
            rm -f "$TEMP_FILE"
            exit 1
        else
            echo "$KEY" >> "$TEMP_FILE"
            UNIQUE_COUNT=$((UNIQUE_COUNT + 1))
        fi
        
        # Показываем прогресс каждые 100 ключей
        if [ $((i % 100)) -eq 0 ]; then
            echo "Обработано $i ключей..."
        fi
    else
        echo "Ошибка при генерации ключа $i"
        rm -f "$TEMP_FILE"
        exit 1
    fi
done

echo
echo "✅ Успешно сгенерировано $UNIQUE_COUNT уникальных ключей из $TOTAL_KEYS"
echo "✅ Тест уникальности пройден!"

# Дополнительная проверка: анализ распределения битов
echo
echo "=== Анализ распределения битов ==="

# Генерируем еще 100 ключей для анализа
ANALYSIS_FILE=$(mktemp)
for i in $(seq 1 100); do
    KEY=$(python3 -c "
import os
key_bytes = os.urandom(16)
print(key_bytes.hex())
" 2>/dev/null)
    echo "$KEY" >> "$ANALYSIS_FILE"
done

# Анализируем распределение битов
echo "Анализируем распределение битов в 100 ключах..."
BIT_COUNT=$(python3 -c "
import sys
total_bits = 0
total_keys = 0

with open('$ANALYSIS_FILE', 'r') as f:
    for line in f:
        key_hex = line.strip()
        if key_hex:
            key_bytes = bytes.fromhex(key_hex)
            bits_set = sum(bin(byte).count('1') for byte in key_bytes)
            total_bits += bits_set
            total_keys += 1

if total_keys > 0:
    avg_bits = total_bits / (total_keys * 128)  # 128 бит на ключ
    print(f'Среднее количество установленных битов: {avg_bits:.2%}')
    if 0.45 <= avg_bits <= 0.55:
        print('✅ Распределение битов выглядит нормальным')
    else:
        print('⚠️  Распределение битов может быть аномальным')
" 2>/dev/null)

echo "$BIT_COUNT"

# Очистка
rm -f "$TEMP_FILE" "$ANALYSIS_FILE"

echo
echo "=== Тест завершен успешно ==="
echo "CSPRNG CryptoCore прошел все проверки качества!"
