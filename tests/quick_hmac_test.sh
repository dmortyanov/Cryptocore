#!/bin/bash
# Быстрый тест HMAC - проверяет RFC 4231 Test Case 1

# Определяем директорию, откуда запущен скрипт
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

CRYPTOCORE="$PROJECT_DIR/cryptocore"
if [ ! -f "$CRYPTOCORE" ]; then
    CRYPTOCORE="$PROJECT_DIR/cryptocore.exe"
fi
if [ ! -f "$CRYPTOCORE" ]; then
    CRYPTOCORE="./cryptocore"
fi
if [ ! -f "$CRYPTOCORE" ]; then
    CRYPTOCORE="./cryptocore.exe"
fi
if [ ! -f "$CRYPTOCORE" ]; then
    if command -v cryptocore >/dev/null 2>&1; then
        CRYPTOCORE="cryptocore"
    elif command -v cryptocore.exe >/dev/null 2>&1; then
        CRYPTOCORE="cryptocore.exe"
    else
        echo "Error: cryptocore executable not found!"
        exit 1
    fi
fi

echo "Quick HMAC Test - RFC 4231 Test Case 1"
echo "======================================"
echo ""

# Создаем тестовый файл
echo -n "Hi There" > /tmp/hmac_test.txt

# Ключ (20 bytes of 0x0b)
KEY="0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"

# Ожидаемый результат
EXPECTED="b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"

echo "Key: $KEY"
echo "Input: Hi There"
echo "Expected: $EXPECTED"
echo ""

# Вычисляем HMAC
RESULT=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY" --input /tmp/hmac_test.txt 2>/dev/null | awk '{print $1}')

echo "Got:      $RESULT"
echo ""

if [ "$RESULT" = "$EXPECTED" ]; then
    echo "✓ Test PASSED!"
    rm -f /tmp/hmac_test.txt
    exit 0
else
    echo "✗ Test FAILED!"
    echo "Expected: $EXPECTED"
    echo "Got:      $RESULT"
    rm -f /tmp/hmac_test.txt
    exit 1
fi

