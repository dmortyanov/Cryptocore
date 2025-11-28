# Быстрый старт: Как протестировать CryptoCore

## 1. Компиляция программы

### Linux/macOS/MSYS2:
```bash
cd /path/to/cryptocore3cource
make clean
make
```

### Windows (PowerShell):
```bash
cd C:\school\kripta\3cource\cryptocore3cource
build.bat
```

Проверьте, что программа скомпилировалась:
```bash
ls cryptocore        # Linux/macOS
ls cryptocore.exe    # Windows
```

## 2. Запуск всех тестов сразу

### Linux/macOS/MSYS2:
```bash
cd tests
./test_all_sprints.sh
```

### Windows (PowerShell):
```powershell
cd tests
.\test_all_sprints.ps1
```

Этот скрипт проверит все спринты:
- ✅ Sprint 1: ECB Mode
- ✅ Sprint 2: CBC, CFB, OFB, CTR
- ✅ Sprint 3: CSPRNG & Auto Key
- ✅ Sprint 4: Hashing (SHA-256, SHA3-256)
- ✅ Sprint 5: HMAC

## 3. Запуск отдельных тестов

### Тесты HMAC (Sprint 5):
```bash
# Linux/macOS/MSYS2
cd tests
./test_hmac.sh

# Windows PowerShell
cd tests
.\test_hmac.ps1
```

### Быстрый тест HMAC (один тест):
```bash
# Linux/macOS/MSYS2
cd tests
./quick_hmac_test.sh

# Windows PowerShell
cd tests
.\quick_hmac_test.ps1
```

### Тесты совместимости с OpenSSL (Sprint 2):
```bash
cd tests
./test_openssl_compat.sh
```

### Тест уникальности ключей (Sprint 3):
```bash
cd tests
./test_csprng_uniqueness.sh
```

## 4. Ручное тестирование

### Пример 1: Шифрование/дешифрование ECB
```bash
# Создаем тестовый файл
echo "Hello, CryptoCore!" > test.txt

# Шифруем
./cryptocore --algorithm aes --mode ecb --encrypt \
  --key 000102030405060708090a0b0c0d0e0f \
  --input test.txt \
  --output test.enc

# Дешифруем
./cryptocore --algorithm aes --mode ecb --decrypt \
  --key 000102030405060708090a0b0c0d0e0f \
  --input test.enc \
  --output test_dec.txt

# Проверяем
diff test.txt test_dec.txt  # Должно быть пусто (файлы идентичны)
```

### Пример 2: Шифрование с автогенерацией ключа
```bash
# Шифруем без указания ключа (ключ сгенерируется автоматически)
./cryptocore --algorithm aes --mode cbc --encrypt \
  --input test.txt \
  --output test.enc

# Ключ будет выведен в консоль, например:
# [INFO] Generated random key: 1a2b3c4d5e6f7890fedcba9876543210

# Используйте этот ключ для дешифрования
./cryptocore --algorithm aes --mode cbc --decrypt \
  --key 1a2b3c4d5e6f7890fedcba9876543210 \
  --input test.enc \
  --output test_dec.txt
```

### Пример 3: Хеширование SHA-256
```bash
# Вычисляем SHA-256
./cryptocore dgst --algorithm sha256 --input test.txt

# Сохраняем в файл
./cryptocore dgst --algorithm sha256 --input test.txt --output test.sha256
```

### Пример 4: HMAC
```bash
# Генерируем HMAC
./cryptocore dgst --algorithm sha256 --hmac \
  --key 00112233445566778899aabbccddeeff \
  --input test.txt

# Сохраняем HMAC в файл
./cryptocore dgst --algorithm sha256 --hmac \
  --key 00112233445566778899aabbccddeeff \
  --input test.txt \
  --output test.hmac

# Проверяем HMAC
./cryptocore dgst --algorithm sha256 --hmac \
  --key 00112233445566778899aabbccddeeff \
  --input test.txt \
  --verify test.hmac
# Должно вывести: [OK] HMAC verification successful
```

### Пример 5: Разные режимы шифрования
```bash
KEY="0123456789abcdef0123456789abcdef"

# CBC
./cryptocore --algorithm aes --mode cbc --encrypt \
  --key "$KEY" --input test.txt --output test_cbc.enc
./cryptocore --algorithm aes --mode cbc --decrypt \
  --key "$KEY" --input test_cbc.enc --output test_cbc_dec.txt

# CFB
./cryptocore --algorithm aes --mode cfb --encrypt \
  --key "$KEY" --input test.txt --output test_cfb.enc
./cryptocore --algorithm aes --mode cfb --decrypt \
  --key "$KEY" --input test_cfb.enc --output test_cfb_dec.txt

# OFB
./cryptocore --algorithm aes --mode ofb --encrypt \
  --key "$KEY" --input test.txt --output test_ofb.enc
./cryptocore --algorithm aes --mode ofb --decrypt \
  --key "$KEY" --input test_ofb.enc --output test_ofb_dec.txt

# CTR
./cryptocore --algorithm aes --mode ctr --encrypt \
  --key "$KEY" --input test.txt --output test_ctr.enc
./cryptocore --algorithm aes --mode ctr --decrypt \
  --key "$KEY" --input test_ctr.enc --output test_ctr_dec.txt
```

## 5. Проверка результатов

После каждого теста проверяйте:
- ✅ Код возврата (0 = успех, не 0 = ошибка)
- ✅ Созданы ли выходные файлы
- ✅ Совпадают ли оригинальные и дешифрованные файлы
- ✅ Правильность хешей и HMAC

## 6. Полезные команды

### Просмотр справки:
```bash
./cryptocore --help
```

### Проверка версии/информации:
```bash
./cryptocore --version  # если реализовано
```

### Сравнение файлов:
```bash
# Linux/macOS
diff file1.txt file2.txt
cmp file1.bin file2.bin

# Windows PowerShell
Compare-Object (Get-Content file1.txt) (Get-Content file2.txt)
```

### Просмотр hex-дампа (для бинарных файлов):
```bash
# Linux/macOS
xxd file.bin | head -20
hexdump -C file.bin | head -20

# Windows PowerShell
Format-Hex file.bin | Select-Object -First 20
```

## 7. Типичные проблемы и решения

### Проблема: "cryptocore: команда не найдена"
**Решение:** Убедитесь, что вы находитесь в правильной директории или используйте полный путь:
```bash
./cryptocore  # или
/path/to/cryptocore3cource/cryptocore
```

### Проблема: "Error: invalid key"
**Решение:** Ключ должен быть 32 hex-символа (16 байт):
```bash
# Правильно:
--key 0123456789abcdef0123456789abcdef

# Неправильно:
--key 0123456789abcdef  # слишком короткий
```

### Проблема: Файлы не совпадают после дешифрования
**Решение:** 
- Проверьте, что используете тот же ключ
- Проверьте, что используете тот же режим
- Для режимов с IV убедитесь, что IV правильный (или не указывайте --iv, чтобы он читался из файла)

### Проблема: Тесты не запускаются
**Решение:**
- Убедитесь, что скрипты имеют права на выполнение: `chmod +x test_*.sh`
- Проверьте, что программа скомпилирована
- Убедитесь, что вы находитесь в директории `tests`

## 8. Структура тестовых файлов

После запуска тестов создаются временные файлы в системной временной директории. Они автоматически удаляются после завершения тестов.

Для ручного тестирования создавайте файлы в текущей директории:
```bash
# Создание тестовых файлов
echo "Test message" > test1.txt
echo -n "Binary data" > test2.bin
dd if=/dev/urandom of=test3.bin bs=1024 count=1  # Linux/macOS
```

## 9. Дополнительная информация

Подробная документация:
- `README.md` - основная документация проекта
- `tests/TESTING_INSTRUCTIONS.md` - детальные инструкции по тестированию
- `tests/README_HMAC_TESTS.md` - документация по тестам HMAC

Примеры использования:
- `EXAMPLES.md` - примеры использования программы
- `QUICKSTART.md` - краткое руководство

