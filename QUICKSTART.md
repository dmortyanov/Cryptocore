# CryptoCore - Краткое руководство

Это руководство поможет вам быстро начать работу с CryptoCore.

## Выберите вашу платформу

### Linux / macOS

```bash
# 1. Установите зависимости (пример для Ubuntu/Debian)
sudo apt-get install build-essential libssl-dev

# 2. Клонируйте/перейдите в репозиторий
cd cryptocore

# 3. Соберите
make

# 4. Протестируйте
echo "Привет, Мир!" > test.txt
./cryptocore --algorithm aes --mode ecb --encrypt --key 0123456789abcdef0123456789abcdef --input test.txt --output test.enc
./cryptocore --algorithm aes --mode ecb --decrypt --key 0123456789abcdef0123456789abcdef --input test.enc --output test.dec
diff test.txt test.dec

# Если diff ничего не выводит, это работает!
```

### Windows

#### Вариант A: MSYS2 (Рекомендуется)

1. **Установите MSYS2** с https://www.msys2.org/

2. **Откройте терминал MSYS2 MINGW64** (важно!)

3. **Установите зависимости:**
   ```bash
   pacman -Syu  # Обновите базу данных пакетов
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-openssl make
   ```

4. **Соберите и протестируйте:**
   ```bash
   cd /c/school/kripta/3cource
   make
   
   echo "Привет, Мир!" > test.txt
   ./cryptocore --algorithm aes --mode ecb --encrypt --key 0123456789abcdef0123456789abcdef --input test.txt --output test.enc
   ./cryptocore --algorithm aes --mode ecb --decrypt --key 0123456789abcdef0123456789abcdef --input test.enc --output test.dec
   diff test.txt test.dec
   ```

#### Вариант B: Предварительно собранный исполняемый файл для Windows

Если вы хотите просто использовать инструмент без сборки:

1. Попросите кого-то с окружением для сборки скомпилировать его для вас
2. Или используйте WSL (Windows Subsystem for Linux) и следуйте инструкциям для Linux

См. [WINDOWS_BUILD.md](WINDOWS_BUILD.md) для более подробных инструкций для Windows.

## Основное использование

### Шифрование файла

```bash
cryptocore --algorithm aes --mode ecb --encrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input secret.txt \
  --output secret.enc
```

### Дешифрование файла

```bash
cryptocore --algorithm aes --mode ecb --decrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input secret.enc \
  --output secret_decrypted.txt
```

## Требования к ключу

- **Длина**: Ровно 32 шестнадцатеричных символа (0-9, a-f)
- **Формат**: Шестнадцатеричная строка (без пробелов, без префикса 0x)
- **Размер**: Представляет 16 байт (128 бит) для AES-128

### Примеры допустимых ключей:
-  `0123456789abcdef0123456789abcdef`
-  `000102030405060708090a0b0c0d0e0f`
-  `FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF`

### Примеры недопустимых ключей:
-  `0123456789abcdef` (слишком короткий - только 16 символов)
-  `0x0123456789abcdef0123456789abcdef` (имеет префикс 0x)
-  `my-secret-key` (не шестнадцатеричный)

## Запуск тестов

### Linux/macOS/MSYS2:
```bash
cd tests
./run_tests.sh
```

### Windows PowerShell:
```powershell
cd tests
.\test_manual.ps1
```

## Устранение неполадок

### Ошибка "cannot find -lcrypto"
- **Linux**: Установите пакет разработки OpenSSL: `sudo apt-get install libssl-dev`
- **macOS**: Установите через Homebrew: `brew install openssl`
- **Windows**: Установите OpenSSL через MSYS2: `pacman -S mingw-w64-x86_64-openssl`

### "make: command not found"
- **Linux/macOS**: Установите make: `sudo apt-get install build-essential`
- **Windows**: Используйте MSYS2 или запустите `build.bat`

### Ошибки отсутствующих DLL на Windows
- Убедитесь, что запускаете из терминала MSYS2 MINGW64, а не из обычного CMD/PowerShell
- Или скопируйте необходимые DLL (libcrypto-*.dll и т.д.) в ту же директорию, что и cryptocore.exe

## Следующие шаги

- Прочитайте полный [README.md](README.md) для подробной документации
- Проверьте [WINDOWS_BUILD.md](WINDOWS_BUILD.md) для инструкций, специфичных для Windows
- Просмотрите директорию [tests/](tests/) для примеров использования
