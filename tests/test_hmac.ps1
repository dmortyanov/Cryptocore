# Тесты HMAC для CryptoCore (PowerShell)
# Проверяет реализацию HMAC согласно требованиям Sprint 5

$ErrorActionPreference = "Stop"

# Путь к исполняемому файлу
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$PROJECT_DIR = Split-Path -Parent $SCRIPT_DIR

$CRYPTOCORE = Join-Path $PROJECT_DIR "cryptocore.exe"
if (-not (Test-Path $CRYPTOCORE)) {
    $CRYPTOCORE = Join-Path $PROJECT_DIR "cryptocore"
}
if (-not (Test-Path $CRYPTOCORE)) {
    $CRYPTOCORE = ".\cryptocore.exe"
}
if (-not (Test-Path $CRYPTOCORE)) {
    $CRYPTOCORE = ".\cryptocore"
}
if (-not (Test-Path $CRYPTOCORE)) {
    $CRYPTOCORE = "cryptocore.exe"
    if (-not (Get-Command $CRYPTOCORE -ErrorAction SilentlyContinue)) {
        $CRYPTOCORE = "cryptocore"
        if (-not (Get-Command $CRYPTOCORE -ErrorAction SilentlyContinue)) {
            Write-Host "Error: cryptocore executable not found!" -ForegroundColor Red
            Write-Host "Please compile the program first: make or build.bat" -ForegroundColor Yellow
            exit 1
        }
    }
}

Write-Host "Using cryptocore: $CRYPTOCORE"

$PASSED = 0
$FAILED = 0

# Функция для проверки результата
function Check-Result {
    param(
        [string]$TestName,
        [string]$Expected,
        [string]$Actual
    )
    
    if ($Actual -eq $Expected) {
        Write-Host "✓ $TestName" -ForegroundColor Green
        $script:PASSED++
        return $true
    } else {
        Write-Host "✗ $TestName" -ForegroundColor Red
        Write-Host "  Expected: $Expected" -ForegroundColor Yellow
        Write-Host "  Got:      $Actual" -ForegroundColor Yellow
        $script:FAILED++
        return $false
    }
}

# Функция для проверки успешного выполнения
function Check-Success {
    param([string]$TestName)
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ $TestName" -ForegroundColor Green
        $script:PASSED++
        return $true
    } else {
        Write-Host "✗ $TestName" -ForegroundColor Red
        $script:FAILED++
        return $false
    }
}

# Функция для проверки неуспешного выполнения
function Check-Failure {
    param([string]$TestName)
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "✓ $TestName" -ForegroundColor Green
        $script:PASSED++
        return $true
    } else {
        Write-Host "✗ $TestName" -ForegroundColor Red
        $script:FAILED++
        return $false
    }
}

Write-Host "=========================================="
Write-Host "  HMAC Tests for CryptoCore"
Write-Host "=========================================="
Write-Host ""

# Создаем временную директорию для тестов
$TEST_DIR = New-TemporaryFile | ForEach-Object { Remove-Item $_; New-Item -ItemType Directory -Path $_ }
Set-Location $TEST_DIR

Write-Host "Test directory: $TEST_DIR"
Write-Host ""

# ============================================
# TEST-1: RFC 4231 Test Case 1
# ============================================
Write-Host "=== TEST-1: RFC 4231 Test Case 1 ==="
"Hi There" | Out-File -FilePath test1.txt -Encoding ASCII -NoNewline
$KEY1 = "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"
$EXPECTED1 = "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"
$RESULT1 = & $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY1 --input test1.txt 2>$null | ForEach-Object { ($_ -split '\s+')[0] }
Check-Result "RFC 4231 Test Case 1" $EXPECTED1 $RESULT1
Write-Host ""

# ============================================
# TEST-2: RFC 4231 Test Case 2
# ============================================
Write-Host "=== TEST-2: RFC 4231 Test Case 2 ==="
"what do ya want for nothing?" | Out-File -FilePath test2.txt -Encoding ASCII -NoNewline
$KEY2 = "4a656665"  # "Jefe"
$EXPECTED2 = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"
$RESULT2 = & $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY2 --input test2.txt 2>$null | ForEach-Object { ($_ -split '\s+')[0] }
Check-Result "RFC 4231 Test Case 2" $EXPECTED2 $RESULT2
Write-Host ""

# ============================================
# TEST-5: Verification Test
# ============================================
Write-Host "=== TEST-5: HMAC Verification ==="
"test message" | Out-File -FilePath verify_test.txt -Encoding ASCII
$KEY5 = "00112233445566778899aabbccddeeff"
& $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY5 --input verify_test.txt --output verify_test.hmac 2>$null | Out-Null
& $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY5 --input verify_test.txt --verify verify_test.hmac 2>$null | Out-Null
Check-Success "HMAC verification with correct key"
Write-Host ""

# ============================================
# TEST-6: Tamper Detection - File
# ============================================
Write-Host "=== TEST-6: Tamper Detection (File) ==="
"original content" | Out-File -FilePath tamper_file.txt -Encoding ASCII
& $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY5 --input tamper_file.txt --output tamper_file.hmac 2>$null | Out-Null
"modified content" | Out-File -FilePath tamper_file.txt -Encoding ASCII
& $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY5 --input tamper_file.txt --verify tamper_file.hmac 2>$null | Out-Null
Check-Failure "Tamper detection (file modified)"
Write-Host ""

# ============================================
# TEST-7: Tamper Detection - Key
# ============================================
Write-Host "=== TEST-7: Tamper Detection (Key) ==="
"test message" | Out-File -FilePath wrong_key_test.txt -Encoding ASCII
$WRONG_KEY = "ffeeddccbbaa99887766554433221100"
& $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY5 --input wrong_key_test.txt --output wrong_key_test.hmac 2>$null | Out-Null
& $CRYPTOCORE dgst --algorithm sha256 --hmac --key $WRONG_KEY --input wrong_key_test.txt --verify wrong_key_test.hmac 2>$null | Out-Null
Check-Failure "Tamper detection (wrong key)"
Write-Host ""

# ============================================
# TEST-8: Key Size Tests
# ============================================
Write-Host "=== TEST-8: Key Size Tests ==="
"test message" | Out-File -FilePath keysize_test.txt -Encoding ASCII

# Key shorter than block size (16 bytes)
$KEY_SHORT = "00112233445566778899aabbccddeeff"
$RESULT_SHORT = & $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY_SHORT --input keysize_test.txt 2>$null | ForEach-Object { ($_ -split '\s+')[0] }
if ($RESULT_SHORT -and $RESULT_SHORT.Length -eq 64) {
    Write-Host "✓ Key < 64 bytes (16 bytes)" -ForegroundColor Green
    $PASSED++
} else {
    Write-Host "✗ Key < 64 bytes (16 bytes)" -ForegroundColor Red
    $FAILED++
}

# Key equal to block size (64 bytes = 128 hex chars)
$KEY_EQUAL = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
$RESULT_EQUAL = & $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY_EQUAL --input keysize_test.txt 2>$null | ForEach-Object { ($_ -split '\s+')[0] }
if ($RESULT_EQUAL -and $RESULT_EQUAL.Length -eq 64) {
    Write-Host "✓ Key = 64 bytes" -ForegroundColor Green
    $PASSED++
} else {
    Write-Host "✗ Key = 64 bytes" -ForegroundColor Red
    $FAILED++
}

# Key longer than block size (100 bytes = 200 hex chars)
$KEY_LONG = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
$RESULT_LONG = & $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY_LONG --input keysize_test.txt 2>$null | ForEach-Object { ($_ -split '\s+')[0] }
if ($RESULT_LONG -and $RESULT_LONG.Length -eq 64) {
    Write-Host "✓ Key > 64 bytes (100 bytes)" -ForegroundColor Green
    $PASSED++
} else {
    Write-Host "✗ Key > 64 bytes (100 bytes)" -ForegroundColor Red
    $FAILED++
}
Write-Host ""

# ============================================
# TEST-9: Empty File Test
# ============================================
Write-Host "=== TEST-9: Empty File Test ==="
New-Item -ItemType File -Path empty_test.txt -Force | Out-Null
$KEY_EMPTY = "00112233445566778899aabbccddeeff"
$RESULT_EMPTY = & $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY_EMPTY --input empty_test.txt 2>$null | ForEach-Object { ($_ -split '\s+')[0] }
if ($RESULT_EMPTY -and $RESULT_EMPTY.Length -eq 64) {
    Write-Host "✓ Empty file HMAC" -ForegroundColor Green
    $PASSED++
} else {
    Write-Host "✗ Empty file HMAC" -ForegroundColor Red
    $FAILED++
}
Write-Host ""

# ============================================
# Итоги
# ============================================
Write-Host "=========================================="
Write-Host "  Test Results"
Write-Host "=========================================="
Write-Host "Passed: $PASSED" -ForegroundColor Green
if ($FAILED -gt 0) {
    Write-Host "Failed: $FAILED" -ForegroundColor Red
    exit 1
} else {
    Write-Host "Failed: $FAILED" -ForegroundColor Green
    exit 0
}

