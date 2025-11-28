# Инструкции по тестированию CryptoCore

## Важно: Перекомпиляция

**Перед запуском тестов необходимо перекомпилировать программу!**

### Linux/macOS/MSYS2:
```bash
make clean
make
```

### Windows:
```bash
build.bat
```

## Комплексные тесты для всех спринтов

Для запуска всех тестов сразу используйте:

### Linux/macOS/MSYS2:
```bash
cd tests
./test_all_sprints.sh
```

### Windows PowerShell:
```powershell
cd tests
.\test_all_sprints.ps1
```

Эти тесты проверяют функциональность всех спринтов:
- Sprint 1: ECB Mode & Basic Encryption
- Sprint 2: CBC, CFB, OFB, CTR Modes
- Sprint 3: CSPRNG & Auto Key Generation
- Sprint 4: Hashing (SHA-256, SHA3-256)
- Sprint 5: HMAC

---
## Проверка поддержки HMAC

Перед запуском тестов убедитесь, что программа поддерживает `--hmac`:

```bash
./cryptocore dgst --algorithm sha256 --hmac --key 00112233445566778899aabbccddeeff --input test.txt
```

Если вы видите ошибку "Unknown argument '--hmac'", значит программа не была перекомпилирована.

## Запуск тестов

### Полный набор тестов

**Linux/macOS/MSYS2:**
```bash
cd tests
chmod +x test_hmac.sh
./test_hmac.sh
```

**Windows PowerShell:**
```powershell
cd tests
.\test_hmac.ps1
```

### Быстрый тест (один RFC 4231 тест)

**Linux/macOS/MSYS2:**
```bash
cd tests
chmod +x quick_hmac_test.sh
./quick_hmac_test.sh
```

**Windows PowerShell:**
```powershell
cd tests
.\quick_hmac_test.ps1
```

### Диагностический тест

**Linux/macOS/MSYS2:**
```bash
cd tests
chmod +x debug_hmac.sh
./debug_hmac.sh
```

## Что делать, если тесты не проходят

1. **Убедитесь, что программа перекомпилирована** с поддержкой HMAC
2. **Проверьте, что программа находится в правильной директории** (в корне проекта или в PATH)
3. **Запустите диагностический тест** для просмотра подробного вывода
4. **Проверьте вывод ошибок** - тесты теперь показывают stderr

## Ожидаемые результаты

При успешном прохождении всех тестов:
- RFC 4231 Test Cases 1-4 должны пройти
- Все тесты верификации должны пройти
- Тесты обнаружения изменений должны правильно определять ошибки

## Проблемы и решения

### Проблема: "Unknown argument '--hmac'"
**Решение:** Перекомпилируйте программу (`make` или `build.bat`)

### Проблема: Пустой результат HMAC
**Решение:** 
- Проверьте, что файл существует и доступен для чтения
- Проверьте вывод ошибок (stderr)
- Убедитесь, что программа была перекомпилирована

### Проблема: Неправильный HMAC для RFC 4231
**Решение:** 
- Проверьте реализацию HMAC в `src/mac/hmac.c`
- Убедитесь, что используется правильная формула RFC 2104
- Проверьте обработку ключей

