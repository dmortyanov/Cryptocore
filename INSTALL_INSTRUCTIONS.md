# Инструкции по установке CryptoCore

### Шаг 1: Скачайте и установите MSYS2

1. Перейдите на https://www.msys2.org/
2. Скачайте установщик (msys2-x86_64-*.exe)
3. Запустите установщик и следуйте мастеру установки
4. После завершения **откройте "MSYS2 MINGW64"** из меню Пуск (важно: используйте MINGW64, а не MSYS2)

### Шаг 2: Установите необходимые пакеты

В терминале MSYS2 MINGW64 выполните:

```bash
# Обновите базу данных пакетов
pacman -Syu

# Если терминал закроется, откройте его снова и выполните:
pacman -Su

# Установите инструменты сборки и OpenSSL
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-openssl make
```

### Шаг 3: Соберите CryptoCore

```bash
# Перейдите в директорию проекта
cd /c/school/kripta/3cource

# Соберите проект
make

# Исполняемый файл будет создан: cryptocore.exe
```

### Шаг 4: Протестируйте установку

```bash
# Создайте тестовый файл
echo "Привет, CryptoCore!" > test.txt

# Зашифруйте его
./cryptocore --algorithm aes --mode ecb --encrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input test.txt \
  --output test.enc

# Расшифруйте его
./cryptocore --algorithm aes --mode ecb --decrypt \
  --key 0123456789abcdef0123456789abcdef \
  --input test.enc \
  --output test_dec.txt

# Проверьте (не должно показывать различий)
diff test.txt test_dec.txt
```

### Шаг 5: Запустите автоматизированные тесты

```bash
cd tests
./run_tests.sh
```

---

## Альтернатива: Использование предсобранного OpenSSL

Если у вас установлен MinGW/GCC, но вы не хотите использовать MSYS2:

### Вариант A: Скачайте предсобранный OpenSSL

1. Скачайте OpenSSL для Windows с: https://slproweb.com/products/Win32OpenSSL.html
2. Установите **Win64 OpenSSL** (не "Light" версию)
3. Запомните путь установки (обычно `C:\OpenSSL-Win64`)

### Вариант B: Сборка с пользовательским путем OpenSSL

Отредактируйте `build.bat` или скомпилируйте вручную:

```cmd
gcc -Wall -Wextra -O2 -I. -IC:\OpenSSL-Win64\include -c main.c -o build\main.o
gcc -Wall -Wextra -O2 -I. -IC:\OpenSSL-Win64\include -c src\ecb.c -o build\ecb.o
gcc -Wall -Wextra -O2 -I. -IC:\OpenSSL-Win64\include -c src\file_io.c -o build\file_io.o
gcc build\main.o build\ecb.o build\file_io.o -o cryptocore.exe -LC:\OpenSSL-Win64\lib -lcrypto
```

---

## Установка на Linux / macOS

### Ubuntu/Debian

```bash
# Установите зависимости
sudo apt-get update
sudo apt-get install build-essential libssl-dev

# Соберите
cd /path/to/cryptocore
make

# Установите в систему (опционально)
sudo make install
```

### Fedora/RHEL

```bash
# Установите зависимости
sudo dnf install gcc make openssl-devel

# Соберите
cd /path/to/cryptocore
make
```

### macOS

```bash
# Установите Xcode Command Line Tools (если еще не установлены)
xcode-select --install

# Установите Homebrew (если еще не установлен)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Установите OpenSSL (обычно уже установлен)
brew install openssl

# Соберите
cd /path/to/cryptocore
make
```

---

## Проверка

После сборки проверьте установку:

```bash
# Проверьте, что исполняемый файл существует
ls -lh cryptocore*

# Запустите с --help (или без аргументов) для просмотра использования
./cryptocore

# Запустите быстрый тест
echo "Тестовое сообщение" > test.txt
./cryptocore --algorithm aes --mode ecb --encrypt --key 0123456789abcdef0123456789abcdef --input test.txt --output test.enc
./cryptocore --algorithm aes --mode ecb --decrypt --key 0123456789abcdef0123456789abcdef --input test.enc --output test_dec.txt
diff test.txt test_dec.txt
```

Если `diff` не выводит ничего, установка прошла успешно! 

---

## Устранение неполадок

### Проблема: "openssl/aes.h: No such file or directory"

**Решение:** OpenSSL не установлен или не в пути включаемых файлов.
- **MSYS2:** Выполните `pacman -S mingw-w64-x86_64-openssl`
- **Linux:** Выполните `sudo apt-get install libssl-dev`
- **macOS:** Выполните `brew install openssl`

### Проблема: "cannot find -lcrypto"

**Решение:** Библиотека OpenSSL не установлена или не в пути библиотек.
- **MSYS2:** Выполните `pacman -S mingw-w64-x86_64-openssl`
- **Linux:** Выполните `sudo apt-get install libssl-dev`

### Проблема: "make: command not found"

**Решение:** 
- **MSYS2:** Выполните `pacman -S make`
- **Windows без MSYS2:** Используйте `build.bat` вместо этого
- **Linux:** Выполните `sudo apt-get install build-essential`

### Проблема: Ошибки отсутствующих DLL при запуске cryptocore.exe

**Решение:**
- Используйте терминал MSYS2 MINGW64 для запуска программы
- Или скопируйте необходимые DLL в ту же директорию, что и cryptocore.exe:
  - libcrypto-*.dll
  - libgcc_*.dll  
  - libwinpthread-*.dll

### Проблема: Тесты не проходят в PowerShell

**Решение:** Используйте специфичный для PowerShell тестовый скрипт:
```powershell
cd tests
.\test_manual.ps1
```

---

## Следующие шаги

После успешной сборки CryptoCore:

1. Прочитайте [README.md](README.md) для полной документации
2. Проверьте [EXAMPLES.md](EXAMPLES.md) для примеров использования
3. Просмотрите [PROJECT_STATUS.md](PROJECT_STATUS.md) для деталей проекта
4. Запустите набор тестов для проверки, что все работает