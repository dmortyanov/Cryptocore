# CryptoCore - Сводка по Спринту 2

## Спринт 2 полностью завершен

Все требования выполнены, код протестирован, документация обновлена.

---

## Что было реализовано

### 1. Новые режимы шифрования (4 режима)

 **CBC (Cipher Block Chaining)**
- Блочный режим с цепочкой
- Использует IV
- Требует дополнение PKCS#7
- Файл: `src/modes/cbc.c`

 **CFB (Cipher Feedback)**
- Потоковый шифр
- Использует IV
- Без дополнения
- Файл: `src/modes/cfb.c`

 **OFB (Output Feedback)**
- Потоковый шифр
- Использует IV
- Без дополнения
- Файл: `src/modes/ofb.c`

 **CTR (Counter)**
- Потоковый шифр
- Использует IV как счетчик
- Без дополнения
- Файл: `src/modes/ctr.c`

### 2. Обработка вектора инициализации (IV)

 **Генерация IV**
- Криптографически стойкий генератор (RAND_bytes из OpenSSL)
- Автоматическая генерация при шифровании
- Файл: `src/modes/utils.c`

 **Формат файла**
```
[16 байт IV] + [Шифртекст]
```

 **Поддержка CLI**
- `--iv` для явного указания IV при дешифровании
- Автоматическое чтение IV из файла

### 3. Обновленный CLI

 **Новые опции:**
```bash
--mode cbc|cfb|ofb|ctr    # Выбор режима
--iv HEXSTRING             # Явный IV (только для дешифрования)
```

 **Примеры использования:**

```bash
# Шифрование (IV генерируется автоматически)
cryptocore --algorithm aes --mode cbc --encrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input file.txt --output file.enc

# Дешифрование (IV из файла)
cryptocore --algorithm aes --mode cbc --decrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input file.enc --output file.dec

# Дешифрование (явный IV)
cryptocore --algorithm aes --mode cbc --decrypt \
  --key 0123456789abcdef0123456789abcdef \
  --iv aabbccddeeff00112233445566778899 \
  --input file.enc --output file.dec
```

### 4. Совместимость с OpenSSL

 **Полная совместимость**
- CryptoCore → OpenSSL ✓
- OpenSSL → CryptoCore ✓
- Все режимы протестированы

 **Тестовый скрипт:** `tests/test_openssl_compat.sh`

### 5. Документация

 **Обновленные документы:**
- `README.md` - Полная документация по всем режимам
- `SPRINT2_STATUS.md` - Подробный отчет о статусе
- `SPRINT2_SUMMARY.md` - Эта сводка

---

##  Структура новых файлов

```
cryptocore/
├── include/
│   └── modes.h              ← НОВЫЙ: Заголовки для режимов
├── src/
│   └── modes/               ← НОВАЯ ДИРЕКТОРИЯ
│       ├── cbc.c           ← CBC режим
│       ├── cfb.c           ← CFB режим
│       ├── ofb.c           ← OFB режим
│       ├── ctr.c           ← CTR режим
│       └── utils.c         ← XOR и генерация IV
├── tests/
│   └── test_openssl_compat.sh  ← НОВЫЙ: Тесты совместимости
├── main.c                   ← ОБНОВЛЕН: Поддержка IV и новых режимов
├── Makefile                 ← ОБНОВЛЕН: Новые файлы
├── build.bat                ← ОБНОВЛЕН: Новые файлы
├── README.md                ← ОБНОВЛЕН: Документация
├── SPRINT2_STATUS.md        ← НОВЫЙ: Отчет
└── SPRINT2_SUMMARY.md       ← НОВЫЙ: Эта сводка
```

---

## Тестирование

### Автоматические тесты

1. **Базовые тесты** (`tests/run_tests.sh`)
   - Тесты полного цикла для ECB
   - Можно расширить для новых режимов

2. **Тесты совместимости с OpenSSL** (`tests/test_openssl_compat.sh`)
   - Тест: CryptoCore → OpenSSL
   - Тест: OpenSSL → CryptoCore
   - Все 5 режимов (ECB, CBC, CFB, OFB, CTR)

### Запуск тестов

```bash
# Linux/macOS/MSYS2
cd tests
./test_openssl_compat.sh

# Windows PowerShell (базовые тесты)
cd tests
.\test_manual.ps1
```

---

## Сборка проекта

### Linux/macOS

```bash
make clean
make
```

### Windows (MSYS2)

```bash
make clean
make
```

### Windows (без Make)

```cmd
build.bat
```

---

##  Статистика

### Новый код

- **Новых файлов:** 8
- **Измененных файлов:** 4
- **Строк кода (новые режимы):** ~500
- **Новых функций:** 13

### Режимы шифрования

| Режим | Строк кода | Поддержка IV | Дополнение |
|-------|-----------|-------------|------------|
| ECB   | ~170      | Нет         | Да         |
| CBC   | ~155      | Да          | Да         |
| CFB   | ~90       | Да          | Нет        |
| OFB   | ~70       | Да          | Нет        |
| CTR   | ~90       | Да          | Нет        |

---

##  Ключевые особенности

### 1. Криптографическая стойкость
-  CSPRNG для генерации IV (RAND_bytes)
-  Корректная реализация всех режимов
-  Правильная обработка дополнения

### 2. Совместимость
-  Полная совместимость с OpenSSL
-  Стандартный формат файлов
-  Кроссплатформенность

### 3. Удобство использования
-  Простой CLI
-  Автоматическая генерация IV
-  Четкие сообщения об ошибках
-  Подробная документация

---

## Примеры использования

### Пример 1: Быстрое шифрование файла

```bash
# Одна команда для шифрования
./cryptocore --algorithm aes --mode cbc --encrypt \
  --key $(openssl rand -hex 16) \
  --input secret.txt

# Создаст secret.txt.enc с автоматически сгенерированным IV
```

### Пример 2: Работа с OpenSSL

```bash
# Шифрование в CryptoCore
./cryptocore --algorithm aes --mode ctr --encrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input data.bin --output data.enc

# Извлечение IV
IV=$(xxd -p -l 16 data.enc | tr -d '\n')

# Дешифрование в OpenSSL
dd if=data.enc of=cipher.bin bs=16 skip=1 2>/dev/null
openssl enc -aes-128-ctr -d \
  -K 0123456789abcdef0123456789abcdef \
  -iv $IV \
  -in cipher.bin \
  -out data.dec
```

### Пример 3: Пакетная обработка

```bash
# Шифрование всех .txt файлов в директории
for file in *.txt; do
    ./cryptocore --algorithm aes --mode cbc --encrypt \
        --key 0123456789abcdef0123456789abcdef \
        --input "$file" --output "${file}.enc"
done
```

---

##  Документация

### Основные документы

1. **[README.md](README.md)** - Полная документация проекта
   - Описание всех режимов
   - Примеры использования
   - Тесты совместимости с OpenSSL

2. **[SPRINT2_STATUS.md](SPRINT2_STATUS.md)** - Подробный отчет
   - Выполнение требований
   - Технические детали
   - Примеры кода

3. **[EXAMPLES.md](EXAMPLES.md)** - Расширенные примеры
   - Различные сценарии использования
   - Лучшие практики

4. **[WINDOWS_BUILD.md](WINDOWS_BUILD.md)** - Сборка на Windows
   - Инструкции для MSYS2
   - Решение проблем

---

## Ограничения

1. Поддерживается только AES-128 (16-байтовый ключ)
2. IV всегда 16 байт (размер блока AES)
3. Нет аутентификации (только конфиденциальность)

---

##  Итоги

###  Все требования Sprint 2 выполнены:

-  4 новых режима реализованы (CBC, CFB, OFB, CTR)
-  Обработка IV полностью реализована
-  CLI расширен
-  Совместимость с OpenSSL подтверждена
-  Документация обновлена
-  Тесты созданы и проходят

###  Готово к использованию:

-  Код компилируется без ошибок
-  Все тесты проходят
-  Совместимость с OpenSSL подтверждена
-  Документация полная и актуальная
-  Кроссплатформенная поддержка (Linux/macOS/Windows)

---

##  Поддержка

Для вопросов и проблем:
1. Читайте [README.md](README.md)
2. Проверьте [EXAMPLES.md](EXAMPLES.md)
3. Смотрите [SPRINT2_STATUS.md](SPRINT2_STATUS.md)