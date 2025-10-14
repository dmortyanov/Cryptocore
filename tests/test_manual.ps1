# Скрипт ручного тестирования CryptoCore для Windows PowerShell
# Этот скрипт выполняет простой тест полного цикла

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  Ручной тест CryptoCore (PowerShell)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

$CRYPTOCORE = "..\cryptocore.exe"
$TEST_KEY = "0123456789abcdef0123456789abcdef"

# Проверка существования cryptocore.exe
if (-not (Test-Path $CRYPTOCORE)) {
    Write-Host "Ошибка: cryptocore.exe не найден. Пожалуйста, сначала соберите проект." -ForegroundColor Red
    Write-Host ""
    Write-Host "Инструкции по сборке:" -ForegroundColor Yellow
    Write-Host "  1. Установите MSYS2 с https://www.msys2.org/" -ForegroundColor Yellow
    Write-Host "  2. Откройте терминал MSYS2 MINGW64" -ForegroundColor Yellow
    Write-Host "  3. Выполните: pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-openssl make" -ForegroundColor Yellow
    Write-Host "  4. Выполните: make" -ForegroundColor Yellow
    exit 1
}

# Тест 1: Базовый тест полного цикла
Write-Host "Тест 1: Базовый тест полного цикла" -ForegroundColor Yellow
Set-Content -Path "test1.txt" -Value "Привет, CryptoCore! Это тест."

& $CRYPTOCORE --algorithm aes --mode ecb --encrypt --key $TEST_KEY --input test1.txt --output test1.enc
if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ Шифрование не удалось" -ForegroundColor Red
    exit 1
}

& $CRYPTOCORE --algorithm aes --mode ecb --decrypt --key $TEST_KEY --input test1.enc --output test1.dec
if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ Дешифрование не удалось" -ForegroundColor Red
    exit 1
}

$original = Get-Content test1.txt -Raw
$decrypted = Get-Content test1.dec -Raw

if ($original -eq $decrypted) {
    Write-Host "✓ Тест 1 ПРОЙДЕН" -ForegroundColor Green
} else {
    Write-Host "✗ Тест 1 НЕ ПРОЙДЕН - Файлы не совпадают" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Тест 2: Большой файл
Write-Host "Тест 2: Тест большого файла" -ForegroundColor Yellow
$content = ""
for ($i = 1; $i -le 50; $i++) {
    $content += "Строка $i: Быстрая коричневая лиса прыгает через ленивую собаку.`n"
}
Set-Content -Path "test2.txt" -Value $content

& $CRYPTOCORE --algorithm aes --mode ecb --encrypt --key $TEST_KEY --input test2.txt --output test2.enc
& $CRYPTOCORE --algorithm aes --mode ecb --decrypt --key $TEST_KEY --input test2.enc --output test2.dec

$original2 = Get-Content test2.txt -Raw
$decrypted2 = Get-Content test2.dec -Raw

if ($original2 -eq $decrypted2) {
    Write-Host "✓ Тест 2 ПРОЙДЕН" -ForegroundColor Green
} else {
    Write-Host "✗ Тест 2 НЕ ПРОЙДЕН" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Очистка
Write-Host "Очистка тестовых файлов..." -ForegroundColor Yellow
Remove-Item -Path "test*.txt", "test*.enc", "test*.dec" -ErrorAction SilentlyContinue
Write-Host ""

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  Все тесты ПРОЙДЕНЫ! ✓" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "CryptoCore работает корректно!" -ForegroundColor Green
