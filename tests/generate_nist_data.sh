#!/bin/bash

# Генерация данных для NIST Statistical Test Suite
# Создает большой файл случайных данных для статистического анализа

echo "=== Генерация данных для NIST STS ==="
echo

OUTPUT_FILE="nist_test_data.bin"
SIZE_MB=10
SIZE_BYTES=$((SIZE_MB * 1024 * 1024))

echo "Генерируем $SIZE_MB МБ случайных данных в файл: $OUTPUT_FILE"
echo "Это может занять некоторое время..."

# Удаляем файл если он существует
rm -f "$OUTPUT_FILE"

# Генерируем данные порциями по 4KB
CHUNK_SIZE=4096
BYTES_WRITTEN=0

while [ $BYTES_WRITTEN -lt $SIZE_BYTES ]; do
    REMAINING=$((SIZE_BYTES - BYTES_WRITTEN))
    CURRENT_CHUNK_SIZE=$((REMAINING < CHUNK_SIZE ? REMAINING : CHUNK_SIZE))
    
    # Генерируем случайные данные
    python3 -c "
import os
import sys
chunk_size = $CURRENT_CHUNK_SIZE
random_data = os.urandom(chunk_size)
sys.stdout.buffer.write(random_data)
" >> "$OUTPUT_FILE"
    
    BYTES_WRITTEN=$((BYTES_WRITTEN + CURRENT_CHUNK_SIZE))
    
    # Показываем прогресс
    PROGRESS=$((BYTES_WRITTEN * 100 / SIZE_BYTES))
    echo -ne "\rПрогресс: $PROGRESS% ($BYTES_WRITTEN / $SIZE_BYTES байт)"
done

echo
echo
echo "✅ Файл $OUTPUT_FILE успешно создан"
echo "Размер файла: $(ls -lh "$OUTPUT_FILE" | awk '{print $5}')"

# Проверяем файл
if [ -f "$OUTPUT_FILE" ] && [ $(stat -c%s "$OUTPUT_FILE") -eq $SIZE_BYTES ]; then
    echo "✅ Размер файла корректен"
else
    echo "❌ Ошибка: размер файла не соответствует ожидаемому"
    exit 1
fi

echo
echo "=== Инструкции по запуску NIST STS ==="
echo
echo "1. Скачайте NIST Statistical Test Suite с официального сайта NIST"
echo "2. Скомпилируйте инструмент:"
echo "   cd sts-2.1.2"
echo "   make"
echo
echo "3. Запустите тесты:"
echo "   ./assess $SIZE_BYTES"
echo
echo "4. При запросе укажите файл: $OUTPUT_FILE"
echo
echo "5. Проанализируйте результаты в директории experiments/AlgorithmTesting/"
echo
echo "Ожидаемые результаты:"
echo "- Большинство тестов должны пройти (p-value ≥ 0.01)"
echo "- Небольшое количество неудач статистически ожидаемо"
echo "- Широкие неудачи указывают на проблемы с RNG"
echo
echo "Файл готов для тестирования: $OUTPUT_FILE"
