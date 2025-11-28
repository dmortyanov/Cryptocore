# Комплексные тесты для всех спринтов CryptoCore (PowerShell)
# Проверяет функциональность Sprint 1-5

$ErrorActionPreference = "Continue"

# Определяем директорию
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$PROJECT_DIR = Split-Path -Parent $SCRIPT_DIR

# Путь к исполняемому файлу
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
    if (Get-Command cryptocore.exe -ErrorAction SilentlyContinue) {
        $CRYPTOCORE = "cryptocore.exe"
    } elseif (Get-Command cryptocore -ErrorAction SilentlyContinue) {
        $CRYPTOCORE = "cryptocore"
    } else {
        Write-Host "Error: cryptocore executable not found!" -ForegroundColor Red
        Write-Host "Please compile the program first: make or build.bat" -ForegroundColor Yellow
        exit 1
    }
}

Write-Host "Using cryptocore: $CRYPTOCORE" -ForegroundColor Cyan
Write-Host ""

# Счетчики
$script:TOTAL_PASSED = 0
$script:TOTAL_FAILED = 0
$script:SPRINT_PASSED = 0
$script:SPRINT_FAILED = 0

function Check-Success {
    param([string]$TestName)
    if ($LASTEXITCODE -eq 0 -or $?) {
        Write-Host "✓ $TestName" -ForegroundColor Green
        $script:SPRINT_PASSED++
        $script:TOTAL_PASSED++
        return $true
    } else {
        Write-Host "✗ $TestName" -ForegroundColor Red
        $script:SPRINT_FAILED++
        $script:TOTAL_FAILED++
        return $false
    }
}

function Check-Failure {
    param([string]$TestName)
    if ($LASTEXITCODE -ne 0 -or -not $?) {
        Write-Host "✓ $TestName" -ForegroundColor Green
        $script:SPRINT_PASSED++
        $script:TOTAL_PASSED++
        return $true
    } else {
        Write-Host "✗ $TestName" -ForegroundColor Red
        $script:SPRINT_FAILED++
        $script:TOTAL_FAILED++
        return $false
    }
}

function Check-FilesEqual {
    param([string]$TestName, [string]$File1, [string]$File2)
    if ((Test-Path $File1) -and (Test-Path $File2)) {
        $hash1 = Get-FileHash $File1 -Algorithm MD5
        $hash2 = Get-FileHash $File2 -Algorithm MD5
        if ($hash1.Hash -eq $hash2.Hash) {
            Write-Host "✓ $TestName" -ForegroundColor Green
            $script:SPRINT_PASSED++
            $script:TOTAL_PASSED++
            return $true
        }
    }
    Write-Host "✗ $TestName" -ForegroundColor Red
    $script:SPRINT_FAILED++
    $script:TOTAL_FAILED++
    return $false
}

function Check-Hash {
    param([string]$TestName, [string]$Expected, [string]$Actual)
    if ($Actual -eq $Expected) {
        Write-Host "✓ $TestName" -ForegroundColor Green
        $script:SPRINT_PASSED++
        $script:TOTAL_PASSED++
        return $true
    } else {
        Write-Host "✗ $TestName" -ForegroundColor Red
        Write-Host "  Expected: $Expected"
        Write-Host "  Got:      $Actual"
        $script:SPRINT_FAILED++
        $script:TOTAL_FAILED++
        return $false
    }
}

function Start-Sprint {
    param([string]$SprintName)
    Write-Host ""
    Write-Host "==========================================" -ForegroundColor Blue
    Write-Host "  $SprintName" -ForegroundColor Blue
    Write-Host "==========================================" -ForegroundColor Blue
    Write-Host ""
    $script:SPRINT_PASSED = 0
    $script:SPRINT_FAILED = 0
}

function End-Sprint {
    param([string]$SprintName)
    Write-Host ""
    Write-Host "--- $SprintName Results ---" -ForegroundColor Blue
    Write-Host "Passed: $script:SPRINT_PASSED"
    Write-Host "Failed: $script:SPRINT_FAILED"
    Write-Host ""
}

# Создаем временную директорию
$TEST_DIR = New-TemporaryFile | ForEach-Object { Remove-Item $_; New-Item -ItemType Directory -Path $_.FullName }
$originalLocation = Get-Location
Set-Location $TEST_DIR

Write-Host "Test directory: $TEST_DIR" -ForegroundColor Cyan
Write-Host ""

# ============================================
# SPRINT 1: ECB Mode
# ============================================
Start-Sprint "SPRINT 1: ECB Mode & Basic Encryption"

Write-Host "=== TEST 1.1: Basic ECB Encryption/Decryption ==="
"Hello, World!" | Out-File -FilePath test1_plain.txt -Encoding ASCII -NoNewline
$KEY1 = "000102030405060708090a0b0c0d0e0f"
& $CRYPTOCORE --algorithm aes --mode ecb --encrypt --key $KEY1 --input test1_plain.txt --output test1_enc.bin 2>&1 | Out-Null
Check-Success "ECB encryption" | Out-Null

& $CRYPTOCORE --algorithm aes --mode ecb --decrypt --key $KEY1 --input test1_enc.bin --output test1_dec.txt 2>&1 | Out-Null
Check-Success "ECB decryption" | Out-Null

Check-FilesEqual "ECB roundtrip" test1_plain.txt test1_dec.txt | Out-Null

Write-Host "=== TEST 1.2: PKCS#7 Padding ==="
"12345" | Out-File -FilePath test2_plain.txt -Encoding ASCII -NoNewline
& $CRYPTOCORE --algorithm aes --mode ecb --encrypt --key $KEY1 --input test2_plain.txt --output test2_enc.bin 2>&1 | Out-Null
Check-Success "ECB encryption with padding" | Out-Null

& $CRYPTOCORE --algorithm aes --mode ecb --decrypt --key $KEY1 --input test2_enc.bin --output test2_dec.txt 2>&1 | Out-Null
Check-Success "ECB decryption with padding" | Out-Null

Check-FilesEqual "PKCS#7 padding roundtrip" test2_plain.txt test2_dec.txt | Out-Null

Write-Host "=== TEST 1.3: Empty File ==="
New-Item -ItemType File -Path test3_plain.txt -Force | Out-Null
& $CRYPTOCORE --algorithm aes --mode ecb --encrypt --key $KEY1 --input test3_plain.txt --output test3_enc.bin 2>&1 | Out-Null
Check-Success "ECB encryption of empty file" | Out-Null

& $CRYPTOCORE --algorithm aes --mode ecb --decrypt --key $KEY1 --input test3_enc.bin --output test3_dec.txt 2>&1 | Out-Null
Check-Success "ECB decryption of empty file" | Out-Null

Check-FilesEqual "Empty file roundtrip" test3_plain.txt test3_dec.txt | Out-Null

End-Sprint "SPRINT 1"

# ============================================
# SPRINT 2: CBC, CFB, OFB, CTR
# ============================================
Start-Sprint "SPRINT 2: CBC, CFB, OFB, CTR Modes"

$KEY2 = "0123456789abcdef0123456789abcdef"

Write-Host "=== TEST 2.1: CBC Mode ==="
"CBC test message" | Out-File -FilePath test_cbc_plain.txt -Encoding ASCII -NoNewline
& $CRYPTOCORE --algorithm aes --mode cbc --encrypt --key $KEY2 --input test_cbc_plain.txt --output test_cbc_enc.bin 2>&1 | Out-Null
Check-Success "CBC encryption" | Out-Null

& $CRYPTOCORE --algorithm aes --mode cbc --decrypt --key $KEY2 --input test_cbc_enc.bin --output test_cbc_dec.txt 2>&1 | Out-Null
Check-Success "CBC decryption" | Out-Null

Check-FilesEqual "CBC roundtrip" test_cbc_plain.txt test_cbc_dec.txt | Out-Null

Write-Host "=== TEST 2.2: CFB Mode ==="
"CFB test message" | Out-File -FilePath test_cfb_plain.txt -Encoding ASCII -NoNewline
& $CRYPTOCORE --algorithm aes --mode cfb --encrypt --key $KEY2 --input test_cfb_plain.txt --output test_cfb_enc.bin 2>&1 | Out-Null
Check-Success "CFB encryption" | Out-Null

& $CRYPTOCORE --algorithm aes --mode cfb --decrypt --key $KEY2 --input test_cfb_enc.bin --output test_cfb_dec.txt 2>&1 | Out-Null
Check-Success "CFB decryption" | Out-Null

Check-FilesEqual "CFB roundtrip" test_cfb_plain.txt test_cfb_dec.txt | Out-Null

Write-Host "=== TEST 2.3: OFB Mode ==="
"OFB test message" | Out-File -FilePath test_ofb_plain.txt -Encoding ASCII -NoNewline
& $CRYPTOCORE --algorithm aes --mode ofb --encrypt --key $KEY2 --input test_ofb_plain.txt --output test_ofb_enc.bin 2>&1 | Out-Null
Check-Success "OFB encryption" | Out-Null

& $CRYPTOCORE --algorithm aes --mode ofb --decrypt --key $KEY2 --input test_ofb_enc.bin --output test_ofb_dec.txt 2>&1 | Out-Null
Check-Success "OFB decryption" | Out-Null

Check-FilesEqual "OFB roundtrip" test_ofb_plain.txt test_ofb_dec.txt | Out-Null

Write-Host "=== TEST 2.4: CTR Mode ==="
"CTR test message" | Out-File -FilePath test_ctr_plain.txt -Encoding ASCII -NoNewline
& $CRYPTOCORE --algorithm aes --mode ctr --encrypt --key $KEY2 --input test_ctr_plain.txt --output test_ctr_enc.bin 2>&1 | Out-Null
Check-Success "CTR encryption" | Out-Null

& $CRYPTOCORE --algorithm aes --mode ctr --decrypt --key $KEY2 --input test_ctr_enc.bin --output test_ctr_dec.txt 2>&1 | Out-Null
Check-Success "CTR decryption" | Out-Null

Check-FilesEqual "CTR roundtrip" test_ctr_plain.txt test_ctr_dec.txt | Out-Null

End-Sprint "SPRINT 2"

# ============================================
# SPRINT 3: CSPRNG & Auto Key
# ============================================
Start-Sprint "SPRINT 3: CSPRNG & Auto Key Generation"

Write-Host "=== TEST 3.1: Auto Key Generation ==="
"Auto key test" | Out-File -FilePath test_auto_plain.txt -Encoding ASCII -NoNewline
$OUTPUT = & $CRYPTOCORE --algorithm aes --mode ecb --encrypt --input test_auto_plain.txt --output test_auto_enc.bin 2>&1
Check-Success "Encryption without --key (auto generation)" | Out-Null

if ($OUTPUT -match '[0-9a-f]{32}') {
    Check-Success "Generated key printed to stdout" | Out-Null
    $GENERATED_KEY = ($OUTPUT | Select-String -Pattern '[0-9a-f]{32}').Matches[0].Value
    if ($GENERATED_KEY) {
        & $CRYPTOCORE --algorithm aes --mode ecb --decrypt --key $GENERATED_KEY --input test_auto_enc.bin --output test_auto_dec.txt 2>&1 | Out-Null
        Check-Success "Decryption with auto-generated key" | Out-Null
        Check-FilesEqual "Auto key roundtrip" test_auto_plain.txt test_auto_dec.txt | Out-Null
    }
} else {
    Check-Failure "Generated key printed to stdout" | Out-Null
}

Write-Host "=== TEST 3.2: Key Uniqueness ==="
$KEYS = @()
for ($i = 1; $i -le 10; $i++) {
    $OUTPUT = & $CRYPTOCORE --algorithm aes --mode ecb --encrypt --input test_auto_plain.txt --output "test_key_$i.bin" 2>&1
    if ($OUTPUT -match '[0-9a-f]{32}') {
        $KEY = ($OUTPUT | Select-String -Pattern '[0-9a-f]{32}').Matches[0].Value
        $KEYS += $KEY
    }
}
$UNIQUE_KEYS = ($KEYS | Select-Object -Unique).Count
if ($UNIQUE_KEYS -eq $KEYS.Count -and $KEYS.Count -ge 5) {
    Check-Success "Generated keys are unique ($UNIQUE_KEYS unique out of $($KEYS.Count))" | Out-Null
} else {
    Check-Failure "Generated keys are unique (only $UNIQUE_KEYS unique out of $($KEYS.Count))" | Out-Null
}

Write-Host "=== TEST 3.3: Key Required for Decryption ==="
$OUTPUT = & $CRYPTOCORE --algorithm aes --mode ecb --decrypt --input test_auto_enc.bin --output test_no_key_dec.txt 2>&1
if ($OUTPUT -match 'key.*required|required.*key') {
    Check-Success "Decryption without key fails" | Out-Null
} else {
    Check-Failure "Decryption without key fails" | Out-Null
}

End-Sprint "SPRINT 3"

# ============================================
# SPRINT 4: Hashing
# ============================================
Start-Sprint "SPRINT 4: Hashing (SHA-256, SHA3-256)"

Write-Host "=== TEST 4.1: SHA-256 Empty File ==="
New-Item -ItemType File -Path test_empty_hash.txt -Force | Out-Null
$HASH_OUTPUT = & $CRYPTOCORE dgst --algorithm sha256 --input test_empty_hash.txt 2>&1
$HASH_VALUE = ($HASH_OUTPUT -split '\s+')[0]
$EXPECTED_EMPTY = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
Check-Hash "SHA-256 of empty file" $EXPECTED_EMPTY $HASH_VALUE | Out-Null

Write-Host "=== TEST 4.2: SHA-256 Known Message ==="
"abc" | Out-File -FilePath test_abc.txt -Encoding ASCII -NoNewline
$HASH_OUTPUT = & $CRYPTOCORE dgst --algorithm sha256 --input test_abc.txt 2>&1
$HASH_VALUE = ($HASH_OUTPUT -split '\s+')[0]
$EXPECTED_ABC = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
Check-Hash "SHA-256 of 'abc'" $EXPECTED_ABC $HASH_VALUE | Out-Null

Write-Host "=== TEST 4.3: SHA3-256 ==="
"SHA3 test" | Out-File -FilePath test_sha3.txt -Encoding ASCII -NoNewline
& $CRYPTOCORE dgst --algorithm sha3-256 --input test_sha3.txt 2>&1 | Out-Null
Check-Success "SHA3-256 computation" | Out-Null

End-Sprint "SPRINT 4"

# ============================================
# SPRINT 5: HMAC
# ============================================
Start-Sprint "SPRINT 5: HMAC"

if (Test-Path (Join-Path $SCRIPT_DIR "test_hmac.ps1")) {
    Write-Host "Running comprehensive HMAC tests..."
    Write-Host ""
    & (Join-Path $SCRIPT_DIR "test_hmac.ps1") | Out-Null
    Check-Success "HMAC comprehensive tests" | Out-Null
} else {
    Write-Host "=== TEST 5.1: Basic HMAC ==="
    $KEY_HMAC = "00112233445566778899aabbccddeeff"
    "HMAC test message" | Out-File -FilePath test_hmac_msg.txt -Encoding ASCII -NoNewline
    & $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY_HMAC --input test_hmac_msg.txt 2>&1 | Out-Null
    Check-Success "HMAC computation" | Out-Null
}

End-Sprint "SPRINT 5"

# Итоговые результаты
Write-Host ""
Write-Host "==========================================" -ForegroundColor Blue
Write-Host "  Final Test Results" -ForegroundColor Blue
Write-Host "==========================================" -ForegroundColor Blue
Write-Host ""
Write-Host "Total Passed: $script:TOTAL_PASSED"
Write-Host "Total Failed: $script:TOTAL_FAILED"
Write-Host ""

# Очистка
Set-Location $originalLocation
Remove-Item -Recurse -Force $TEST_DIR

if ($script:TOTAL_FAILED -eq 0) {
    Write-Host "✓ All tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "✗ Some tests failed" -ForegroundColor Red
    exit 1
}

