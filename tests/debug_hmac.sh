#!/bin/bash
# Диагностический скрипт для проверки HMAC

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

echo "=== HMAC Debug Test ==="
echo ""

# Проверка наличия программы
if [ ! -f "$CRYPTOCORE" ]; then
    echo "ERROR: cryptocore not found!"
    exit 1
fi

echo "1. Testing if --hmac flag is recognized:"
$CRYPTOCORE dgst --algorithm sha256 --hmac --help 2>&1 | head -5
echo ""

# Создаем тестовый файл
echo -n "Hi There" > /tmp/hmac_debug.txt
echo "2. Created test file: /tmp/hmac_debug.txt"
echo ""

# Тест 1: Простой HMAC
echo "3. Testing HMAC computation:"
KEY="0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"
echo "   Command: $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY --input /tmp/hmac_debug.txt"
echo "   Output:"
OUTPUT=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY" --input /tmp/hmac_debug.txt 2>&1)
echo "$OUTPUT"
echo ""

# Проверяем результат
RESULT=$(echo "$OUTPUT" | awk '{print $1}')
if [ -n "$RESULT" ] && [ ${#RESULT} -eq 64 ]; then
    echo "   ✓ Got valid HMAC: $RESULT"
    EXPECTED="b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"
    if [ "$RESULT" = "$EXPECTED" ]; then
        echo "   ✓ Matches RFC 4231 Test Case 1!"
    else
        echo "   ✗ Does NOT match RFC 4231 Test Case 1"
        echo "   Expected: $EXPECTED"
    fi
else
    echo "   ✗ Invalid or missing HMAC output"
    echo "   Result: '$RESULT'"
    echo "   Length: ${#RESULT}"
fi

rm -f /tmp/hmac_debug.txt

