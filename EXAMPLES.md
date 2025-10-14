# Примеры использования CryptoCore

Этот документ содержит практические примеры использования CryptoCore для различных задач шифрования и дешифрования.

## Базовые примеры

### Пример 1: Шифрование простого текстового файла

```bash
# Создайте текстовый файл
echo "Это секретное сообщение." > message.txt

# Зашифруйте его
cryptocore --algorithm aes --mode ecb --encrypt \
  --key 000102030405060708090a0b0c0d0e0f \
  --input message.txt \
  --output message.enc

# Зашифрованный файл message.enc теперь содержит шифртекст
```

### Пример 2: Дешифрование зашифрованного файла

```bash
# Расшифруйте файл
cryptocore --algorithm aes --mode ecb --decrypt \
  --key 000102030405060708090a0b0c0d0e0f \
  --input message.enc \
  --output message_decrypted.txt

# Проверьте, что он совпадает с оригиналом
diff message.txt message_decrypted.txt
```

### Пример 3: Использование имен выходных файлов по умолчанию

```bash
# Шифрование (автоматически создает input.txt.enc)
cryptocore --algorithm aes --mode ecb --encrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input input.txt

# Дешифрование (автоматически создает input.txt.enc.dec)
cryptocore --algorithm aes --mode ecb --decrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input input.txt.enc
```

## Продвинутые примеры

### Пример 4: Шифрование бинарных файлов

```bash
# Зашифруйте файл изображения
cryptocore --algorithm aes --mode ecb --encrypt \
  --key abcdef0123456789abcdef0123456789 \
  --input photo.jpg \
  --output photo.jpg.enc

# Расшифруйте его обратно
cryptocore --algorithm aes --mode ecb --decrypt \
  --key abcdef0123456789abcdef0123456789 \
  --input photo.jpg.enc \
  --output photo_decrypted.jpg

# Проверьте с помощью контрольной суммы
sha256sum photo.jpg photo_decrypted.jpg
```

### Пример 5: Шифрование больших файлов

```bash
# Создайте большой тестовый файл (10MB)
dd if=/dev/urandom of=largefile.bin bs=1M count=10

# Зашифруйте его
cryptocore --algorithm aes --mode ecb --encrypt \
  --key fedcba9876543210fedcba9876543210 \
  --input largefile.bin \
  --output largefile.enc

# Расшифруйте и проверьте
cryptocore --algorithm aes --mode ecb --decrypt \
  --key fedcba9876543210fedcba9876543210 \
  --input largefile.enc \
  --output largefile_dec.bin

# Сравните
cmp largefile.bin largefile_dec.bin && echo "Файлы идентичны!"
```

### Пример 6: Пакетная обработка нескольких файлов

```bash
#!/bin/bash
# encrypt_all.sh - Шифрование нескольких файлов одним ключом

KEY="0123456789abcdef0123456789abcdef"

for file in *.txt; do
    echo "Шифрование $file..."
    cryptocore --algorithm aes --mode ecb --encrypt \
        --key $KEY \
        --input "$file" \
        --output "${file}.enc"
done

echo "Все файлы зашифрованы!"
```

### Пример 7: Генерация ключа

```bash
# Сгенерируйте случайный 16-байтовый (128-битный) ключ в hex-формате
RANDOM_KEY=$(openssl rand -hex 16)
echo "Сгенерированный ключ: $RANDOM_KEY"

# Используйте его для шифрования
cryptocore --algorithm aes --mode ecb --encrypt \
  --key $RANDOM_KEY \
  --input secret.txt \
  --output secret.enc

# Сохраните ключ в файл (храните его в безопасности!)
echo $RANDOM_KEY > secret.key
```

## Примеры для Windows

### Примеры для PowerShell

```powershell
# Зашифруйте файл
.\cryptocore.exe --algorithm aes --mode ecb --encrypt `
  --key 0123456789abcdef0123456789abcdef `
  --input document.txt `
  --output document.enc

# Расшифруйте файл
.\cryptocore.exe --algorithm aes --mode ecb --decrypt `
  --key 0123456789abcdef0123456789abcdef `
  --input document.enc `
  --output document_decrypted.txt

# Проверьте, что файлы совпадают
fc document.txt document_decrypted.txt
```

### Пакетная обработка в PowerShell

```powershell
# Зашифруйте все .txt файлы в текущей директории
$key = "0123456789abcdef0123456789abcdef"

Get-ChildItem *.txt | ForEach-Object {
    Write-Host "Шифрование $($_.Name)..."
    .\cryptocore.exe --algorithm aes --mode ecb --encrypt `
        --key $key `
        --input $_.Name `
        --output "$($_.Name).enc"
}

Write-Host "Все файлы зашифрованы!"
```

## Примеры обработки ошибок

### Пример 8: Обработка неправильных ключей

```bash
# Зашифруйте одним ключом
cryptocore --algorithm aes --mode ecb --encrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input data.txt \
  --output data.enc

# Попытайтесь расшифровать неправильным ключом (выдаст мусор или ошибку дополнения)
cryptocore --algorithm aes --mode ecb --decrypt \
  --key ffffffffffffffffffffffffffffffff \
  --input data.enc \
  --output data_wrong.txt

# Скорее всего не получится или выдаст поврежденные данные
```

### Пример 9: Проверка входных данных

```bash
# Проверьте, существует ли файл перед шифрованием
if [ -f "myfile.txt" ]; then
    cryptocore --algorithm aes --mode ecb --encrypt \
        --key 0123456789abcdef0123456789abcdef \
        --input myfile.txt \
        --output myfile.enc
else
    echo "Ошибка: myfile.txt не найден"
fi
```

## Примеры тестирования и проверки

### Пример 10: Проверка полного цикла

```bash
#!/bin/bash
# round_trip_test.sh

KEY="0123456789abcdef0123456789abcdef"
ORIGINAL="test_original.txt"
ENCRYPTED="test.enc"
DECRYPTED="test_decrypted.txt"

# Создайте тестовый файл
echo "Тест полного цикла CryptoCore" > $ORIGINAL

# Зашифруйте
cryptocore --algorithm aes --mode ecb --encrypt \
    --key $KEY --input $ORIGINAL --output $ENCRYPTED

# Расшифруйте
cryptocore --algorithm aes --mode ecb --decrypt \
    --key $KEY --input $ENCRYPTED --output $DECRYPTED

# Проверьте
if diff $ORIGINAL $DECRYPTED > /dev/null; then
    echo "✓ Тест полного цикла ПРОЙДЕН"
    exit 0
else
    echo "✗ Тест полного цикла НЕ ПРОЙДЕН"
    exit 1
fi
```

### Пример 11: Тестирование производительности

```bash
#!/bin/bash
# performance_test.sh

# Создайте тестовые файлы разных размеров
echo "Создание тестовых файлов..."
dd if=/dev/urandom of=test_1kb.bin bs=1K count=1 2>/dev/null
dd if=/dev/urandom of=test_1mb.bin bs=1M count=1 2>/dev/null
dd if=/dev/urandom of=test_10mb.bin bs=1M count=10 2>/dev/null

KEY="0123456789abcdef0123456789abcdef"

# Тестируйте скорость шифрования
for file in test_*.bin; do
    echo "Шифрование $file..."
    time cryptocore --algorithm aes --mode ecb --encrypt \
        --key $KEY --input $file --output ${file}.enc
done

echo "Тест производительности завершен!"
```

### Пример 12: Сравнение с OpenSSL

```bash
# Примечание: Прямое сравнение сложно из-за различий в дополнении
# CryptoCore всегда использует дополнение PKCS#7

# Создайте файл, выровненный по 16 байтам
echo -n "1234567890abcdef" > aligned.txt

# Зашифруйте с помощью CryptoCore
cryptocore --algorithm aes --mode ecb --encrypt \
    --key 0123456789abcdef0123456789abcdef \
    --input aligned.txt \
    --output crypto_out.bin

# Примечание: CryptoCore добавляет дополнение PKCS#7 даже к выровненным блокам
# Поэтому прямое сравнение с OpenSSL (с -nopad) не совпадет

# Для проверки корректности используйте тест полного цикла вместо этого
cryptocore --algorithm aes --mode ecb --decrypt \
    --key 0123456789abcdef0123456789abcdef \
    --input crypto_out.bin \
    --output crypto_dec.txt

diff aligned.txt crypto_dec.txt
```

## Примеры интеграции

### Пример 13: Использование в скрипте резервного копирования

```bash
#!/bin/bash
# secure_backup.sh - Шифрование файлов перед резервным копированием

BACKUP_DIR="/path/to/backup"
ENCRYPTION_KEY=$(cat /secure/location/backup.key)

# Создайте директорию резервного копирования
mkdir -p $BACKUP_DIR

# Скопируйте и зашифруйте важные файлы
for file in ~/important/*; do
    filename=$(basename "$file")
    cryptocore --algorithm aes --mode ecb --encrypt \
        --key $ENCRYPTION_KEY \
        --input "$file" \
        --output "$BACKUP_DIR/${filename}.enc"
done

echo "Резервное копирование завершено: $BACKUP_DIR"
```

### Пример 14: Безопасная передача файлов

```bash
# Сторона отправителя:
# 1. Зашифруйте файл
cryptocore --algorithm aes --mode ecb --encrypt \
    --key 0123456789abcdef0123456789abcdef \
    --input sensitive_data.csv \
    --output sensitive_data.enc

# 2. Передайте зашифрованный файл (безопасно отправлять по незащищенному каналу)
scp sensitive_data.enc user@remote:/path/

# 3. Поделитесь ключом отдельно (используйте защищенный канал, например Signal, лично и т.д.)

# Сторона получателя:
# 4. Расшифруйте файл
cryptocore --algorithm aes --mode ecb --decrypt \
    --key 0123456789abcdef0123456789abcdef \
    --input sensitive_data.enc \
    --output sensitive_data.csv
```

## Распространенные ошибки и решения

### Ошибка 1: Неправильная длина ключа

```bash
# ✗ Неправильно: Ключ слишком короткий
cryptocore --algorithm aes --mode ecb --encrypt \
    --key 0123456789abcdef \
    --input file.txt
# Ошибка: Ключ AES-128 должен быть 32 шестнадцатеричных символа (16 байт)

# ✓ Правильно: 32 hex-символа
cryptocore --algorithm aes --mode ecb --encrypt \
    --key 0123456789abcdef0123456789abcdef \
    --input file.txt
```

### Ошибка 2: Бинарный против текстового режима

```bash
# CryptoCore всегда использует бинарный режим, что правильно
# Это работает для текстовых и бинарных файлов

# Текстовый файл - работает отлично
cryptocore --algorithm aes --mode ecb --encrypt \
    --key 0123456789abcdef0123456789abcdef \
    --input textfile.txt --output textfile.enc

# Бинарный файл - также работает отлично
cryptocore --algorithm aes --mode ecb --encrypt \
    --key 0123456789abcdef0123456789abcdef \
    --input image.png --output image.png.enc
```

### Ошибка 3: Забыли ключ

```bash
# Всегда сохраняйте ключ в безопасном месте перед шифрованием!

# Хорошая практика: Сохраните ключ перед шифрованием
KEY="0123456789abcdef0123456789abcdef"
echo $KEY > important_file.key  # Храните в безопасности!

cryptocore --algorithm aes --mode ecb --encrypt \
    --key $KEY \
    --input important_file.txt \
    --output important_file.enc

# Без ключа вы не сможете расшифровать!
```

## Лучшие практики безопасности

1. **Никогда не встраивайте ключи в скрипты** - Используйте переменные окружения или файлы ключей
2. **Используйте сильные случайные ключи** - Генерируйте с помощью `openssl rand -hex 16`
3. **Храните ключи в безопасности** - Храните отдельно от зашифрованных данных
4. **Помните: режим ECB не семантически безопасен** - Используйте CBC/CTR/GCM в продакшене
5. **Проверяйте дешифрование** - Всегда сравнивайте контрольные суммы после дешифрования

---

Для получения дополнительной информации см.:
- [README.md](README.md) - Полная документация
- [QUICKSTART.md](QUICKSTART.md) - Руководство по началу работы
- [WINDOWS_BUILD.md](WINDOWS_BUILD.md) - Инструкции для Windows
