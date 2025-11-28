#!/bin/bash
# Тесты HMAC для CryptoCore
# Проверяет реализацию HMAC согласно требованиям Sprint 5

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Путь к исполняемому файлу
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
        echo "Please compile the program first: make"
        exit 1
    fi
fi

echo "Using cryptocore: $CRYPTOCORE"

PASSED=0
FAILED=0

# Функция для проверки результата
check_result() {
    local test_name="$1"
    local expected="$2"
    local actual="$3"
    
    if [ "$actual" = "$expected" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  Expected: $expected"
        echo "  Got:      $actual"
        ((FAILED++))
        return 1
    fi
}

# Функция для проверки успешного выполнения команды
check_success() {
    local test_name="$1"
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}✗${NC} $test_name"
        ((FAILED++))
        return 1
    fi
}

# Функция для проверки неуспешного выполнения команды
check_failure() {
    local test_name="$1"
    if [ $? -ne 0 ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}✗${NC} $test_name"
        ((FAILED++))
        return 1
    fi
}

echo "=========================================="
echo "  HMAC Tests for CryptoCore"
echo "=========================================="
echo ""

# Создаем временную директорию для тестов
TEST_DIR=$(mktemp -d)
trap "rm -rf $TEST_DIR" EXIT
cd "$TEST_DIR"

echo "Test directory: $TEST_DIR"
echo ""

# ============================================
# TEST-1: RFC 4231 Test Case 1
# ============================================
echo "=== TEST-1: RFC 4231 Test Case 1 ==="
echo -n "Hi There" > test1.txt
KEY1="0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"
EXPECTED1="b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"
OUTPUT1=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY1" --input test1.txt 2>&1)
RESULT1=$(echo "$OUTPUT1" | awk '{print $1}')
if [ -z "$RESULT1" ] || [ ${#RESULT1} -ne 64 ]; then
    echo "Debug: Full output:"
    echo "$OUTPUT1"
    echo "Debug: Result length: ${#RESULT1}"
fi
check_result "RFC 4231 Test Case 1" "$EXPECTED1" "$RESULT1"
echo ""

# ============================================
# TEST-2: RFC 4231 Test Case 2
# ============================================
echo "=== TEST-2: RFC 4231 Test Case 2 ==="
echo -n "what do ya want for nothing?" > test2.txt
KEY2="4a656665"  # "Jefe"
EXPECTED2="5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"
OUTPUT2=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY2" --input test2.txt 2>&1)
RESULT2=$(echo "$OUTPUT2" | awk '{print $1}')
if [ -z "$RESULT2" ] || [ ${#RESULT2} -ne 64 ]; then
    echo "Debug: Full output:"
    echo "$OUTPUT2"
fi
check_result "RFC 4231 Test Case 2" "$EXPECTED2" "$RESULT2"
echo ""

# ============================================
# TEST-3: RFC 4231 Test Case 3
# ============================================
echo "=== TEST-3: RFC 4231 Test Case 3 ==="
printf "%b" "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd" > test3.txt
KEY3="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"  # 20 bytes of 0xaa
EXPECTED3="773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"
OUTPUT3=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY3" --input test3.txt 2>&1)
RESULT3=$(echo "$OUTPUT3" | awk '{print $1}')
if [ -z "$RESULT3" ] || [ ${#RESULT3} -ne 64 ]; then
    echo "Debug: Full output:"
    echo "$OUTPUT3"
fi
check_result "RFC 4231 Test Case 3" "$EXPECTED3" "$RESULT3"
echo ""

# ============================================
# TEST-4: RFC 4231 Test Case 4
# ============================================
echo "=== TEST-4: RFC 4231 Test Case 4 ==="
printf "%b" "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd" > test4.txt
KEY4="0102030405060708090a0b0c0d0e0f10111213141516171819"  # 25 bytes
EXPECTED4="82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b"
OUTPUT4=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY4" --input test4.txt 2>&1)
RESULT4=$(echo "$OUTPUT4" | awk '{print $1}')
if [ -z "$RESULT4" ] || [ ${#RESULT4} -ne 64 ]; then
    echo "Debug: Full output:"
    echo "$OUTPUT4"
fi
check_result "RFC 4231 Test Case 4" "$EXPECTED4" "$RESULT4"
echo ""

# ============================================
# TEST-5: Verification Test
# ============================================
echo "=== TEST-5: HMAC Verification ==="
echo "test message" > verify_test.txt
KEY5="00112233445566778899aabbccddeeff"
$CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY5" --input verify_test.txt --output verify_test.hmac > /dev/null 2>&1
$CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY5" --input verify_test.txt --verify verify_test.hmac > /dev/null 2>&1
check_success "HMAC verification with correct key"
echo ""

# ============================================
# TEST-6: Tamper Detection - File
# ============================================
echo "=== TEST-6: Tamper Detection (File) ==="
echo "original content" > tamper_file.txt
$CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY5" --input tamper_file.txt --output tamper_file.hmac > /dev/null 2>&1
echo "modified content" > tamper_file.txt
$CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY5" --input tamper_file.txt --verify tamper_file.hmac > /dev/null 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    echo -e "${GREEN}✓${NC} Tamper detection (file modified)"
    ((PASSED++))
else
    echo -e "${RED}✗${NC} Tamper detection (file modified) - verification should have failed"
    ((FAILED++))
fi
echo ""

# ============================================
# TEST-7: Tamper Detection - Key
# ============================================
echo "=== TEST-7: Tamper Detection (Key) ==="
echo "test message" > wrong_key_test.txt
WRONG_KEY="ffeeddccbbaa99887766554433221100"
$CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY5" --input wrong_key_test.txt --output wrong_key_test.hmac > /dev/null 2>&1
$CRYPTOCORE dgst --algorithm sha256 --hmac --key "$WRONG_KEY" --input wrong_key_test.txt --verify wrong_key_test.hmac > /dev/null 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    echo -e "${GREEN}✓${NC} Tamper detection (wrong key)"
    ((PASSED++))
else
    echo -e "${RED}✗${NC} Tamper detection (wrong key) - verification should have failed"
    ((FAILED++))
fi
echo ""

# ============================================
# TEST-8: Key Size Tests
# ============================================
echo "=== TEST-8: Key Size Tests ==="
echo "test message" > keysize_test.txt

# Key shorter than block size (16 bytes)
KEY_SHORT="00112233445566778899aabbccddeeff"
OUTPUT_SHORT=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY_SHORT" --input keysize_test.txt 2>&1)
RESULT_SHORT=$(echo "$OUTPUT_SHORT" | awk '{print $1}')
if [ -n "$RESULT_SHORT" ] && [ ${#RESULT_SHORT} -eq 64 ]; then
    echo -e "${GREEN}✓${NC} Key < 64 bytes (16 bytes)"
    ((PASSED++))
else
    echo -e "${RED}✗${NC} Key < 64 bytes (16 bytes)"
    ((FAILED++))
fi

# Key equal to block size (64 bytes = 128 hex chars)
KEY_EQUAL="00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
OUTPUT_EQUAL=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY_EQUAL" --input keysize_test.txt 2>&1)
RESULT_EQUAL=$(echo "$OUTPUT_EQUAL" | awk '{print $1}')
if [ -n "$RESULT_EQUAL" ] && [ ${#RESULT_EQUAL} -eq 64 ]; then
    echo -e "${GREEN}✓${NC} Key = 64 bytes"
    ((PASSED++))
else
    echo -e "${RED}✗${NC} Key = 64 bytes"
    ((FAILED++))
fi

# Key longer than block size (100 bytes = 200 hex chars)
KEY_LONG="00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
OUTPUT_LONG=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY_LONG" --input keysize_test.txt 2>&1)
RESULT_LONG=$(echo "$OUTPUT_LONG" | awk '{print $1}')
if [ -n "$RESULT_LONG" ] && [ ${#RESULT_LONG} -eq 64 ]; then
    echo -e "${GREEN}✓${NC} Key > 64 bytes (100 bytes)"
    ((PASSED++))
else
    echo -e "${RED}✗${NC} Key > 64 bytes (100 bytes)"
    ((FAILED++))
fi
echo ""

# ============================================
# TEST-9: Empty File Test
# ============================================
echo "=== TEST-9: Empty File Test ==="
touch empty_test.txt
KEY_EMPTY="00112233445566778899aabbccddeeff"
OUTPUT_EMPTY=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY_EMPTY" --input empty_test.txt 2>&1)
RESULT_EMPTY=$(echo "$OUTPUT_EMPTY" | awk '{print $1}')
if [ -n "$RESULT_EMPTY" ] && [ ${#RESULT_EMPTY} -eq 64 ]; then
    echo -e "${GREEN}✓${NC} Empty file HMAC"
    ((PASSED++))
else
    echo -e "${RED}✗${NC} Empty file HMAC"
    ((FAILED++))
fi
echo ""

# ============================================
# TEST-10: Large File Test
# ============================================
echo "=== TEST-10: Large File Test ==="
# Создаем файл размером ~1MB
dd if=/dev/urandom of=large_test.bin bs=1024 count=1024 > /dev/null 2>&1 || head -c 1048576 /dev/urandom > large_test.bin 2>/dev/null || python3 -c "import os; f=open('large_test.bin','wb'); f.write(os.urandom(1048576)); f.close()" 2>/dev/null
if [ -f large_test.bin ]; then
    KEY_LARGE="00112233445566778899aabbccddeeff"
    OUTPUT_LARGE=$($CRYPTOCORE dgst --algorithm sha256 --hmac --key "$KEY_LARGE" --input large_test.bin 2>&1)
    RESULT_LARGE=$(echo "$OUTPUT_LARGE" | awk '{print $1}')
    if [ -n "$RESULT_LARGE" ] && [ ${#RESULT_LARGE} -eq 64 ]; then
        echo -e "${GREEN}✓${NC} Large file HMAC (~1MB)"
        ((PASSED++))
    else
        echo -e "${RED}✗${NC} Large file HMAC (~1MB)"
        ((FAILED++))
    fi
else
    echo -e "${YELLOW}⚠${NC} Large file test skipped (could not create test file)"
fi
echo ""

# ============================================
# Итоги
# ============================================
echo "=========================================="
echo "  Test Results"
echo "=========================================="
echo -e "${GREEN}Passed: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}Failed: $FAILED${NC}"
    exit 0
fi

