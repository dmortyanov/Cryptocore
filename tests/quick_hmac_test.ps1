# Быстрый тест HMAC - проверяет RFC 4231 Test Case 1

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
    if (Get-Command cryptocore.exe -ErrorAction SilentlyContinue) {
        $CRYPTOCORE = "cryptocore.exe"
    } elseif (Get-Command cryptocore -ErrorAction SilentlyContinue) {
        $CRYPTOCORE = "cryptocore"
    } else {
        Write-Host "Error: cryptocore executable not found!" -ForegroundColor Red
        exit 1
    }
}

Write-Host "Quick HMAC Test - RFC 4231 Test Case 1"
Write-Host "======================================"
Write-Host ""

# Создаем тестовый файл
$TEST_FILE = [System.IO.Path]::GetTempFileName()
"Hi There" | Out-File -FilePath $TEST_FILE -Encoding ASCII -NoNewline

# Ключ (20 bytes of 0x0b)
$KEY = "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"

# Ожидаемый результат
$EXPECTED = "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"

Write-Host "Key: $KEY"
Write-Host "Input: Hi There"
Write-Host "Expected: $EXPECTED"
Write-Host ""

# Вычисляем HMAC
$RESULT = & $CRYPTOCORE dgst --algorithm sha256 --hmac --key $KEY --input $TEST_FILE 2>$null | ForEach-Object { ($_ -split '\s+')[0] }

Write-Host "Got:      $RESULT"
Write-Host ""

if ($RESULT -eq $EXPECTED) {
    Write-Host "✓ Test PASSED!" -ForegroundColor Green
    Remove-Item $TEST_FILE -Force
    exit 0
} else {
    Write-Host "✗ Test FAILED!" -ForegroundColor Red
    Write-Host "Expected: $EXPECTED"
    Write-Host "Got:      $RESULT"
    Remove-Item $TEST_FILE -Force
    exit 1
}

