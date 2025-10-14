# Сборка CryptoCore на Windows

Это руководство содержит инструкции по сборке CryptoCore на Windows.

## Предварительные требования

Вам необходимо установить компилятор C и библиотеку OpenSSL. Рекомендуемый подход - использование MSYS2.

### Вариант 1: MSYS2 (Рекомендуется)

1. **Скачайте и установите MSYS2**
   - Скачайте с: https://www.msys2.org/
   - Запустите установщик и следуйте шагам установки
   - После установки откройте терминал "MSYS2 MINGW64"

2. **Установите необходимые пакеты**
   ```bash
   pacman -Syu  # Обновите базу данных пакетов
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-openssl make
   ```

3. **Соберите проект**
   ```bash
   cd /c/school/kripta/3cource
   make
   ```

### Вариант 2: Использование build.bat (Без Make)

Если у вас установлен MinGW/GCC, но нет Make:

1. **Откройте командную строку или PowerShell**

2. **Перейдите в директорию проекта**
   ```cmd
   cd C:\school\kripta\3cource
   ```

3. **Запустите скрипт сборки**
   ```cmd
   build.bat
   ```

Это создаст `cryptocore.exe` в текущей директории.

### Вариант 3: Ручная компиляция

Если вы предпочитаете компилировать вручную или пакетный скрипт не работает:

```cmd
REM Создайте директорию сборки
mkdir build

REM Скомпилируйте исходные файлы
gcc -Wall -Wextra -O2 -I. -c main.c -o build\main.o
gcc -Wall -Wextra -O2 -I. -c src\ecb.c -o build\ecb.o
gcc -Wall -Wextra -O2 -I. -c src\file_io.c -o build\file_io.o

REM Линковка для создания исполняемого файла
gcc build\main.o build\ecb.o build\file_io.o -o cryptocore.exe -lcrypto
```

## Установка OpenSSL на Windows

### Для MinGW-w64

Если вы используете отдельный MinGW-w64, вам может потребоваться:

1. Скачать OpenSSL для Windows с: https://slproweb.com/products/Win32OpenSSL.html
2. Установить Win64 OpenSSL (не "Light" версию)
3. Добавить директорию `lib` OpenSSL в путь библиотек
4. При компиляции может потребоваться указать пути OpenSSL:
   ```cmd
   gcc ... -I"C:\OpenSSL-Win64\include" -L"C:\OpenSSL-Win64\lib" -lcrypto
   ```

## Устранение неполадок

### "gcc: command not found" или не распознается

- Убедитесь, что MinGW/GCC установлен и добавлен в PATH
- В MSYS2 используйте терминал "MSYS2 MINGW64", а не обычный терминал MSYS2

### Ошибка линкера: cannot find -lcrypto

- OpenSSL не установлен или не в пути библиотек
- Установите OpenSSL, как описано выше
- Убедитесь, что используете правильный терминал (MINGW64 в MSYS2)

### Отсутствует DLL при запуске cryptocore.exe

- Скопируйте необходимые DLL в ту же директорию, что и cryptocore.exe:
  - `libcrypto-*.dll` (из OpenSSL)
  - `libgcc_*.dll` (из MinGW)
  - `libwinpthread-*.dll` (из MinGW)
- Или добавьте директории bin MinGW и OpenSSL в ваш PATH

## Тестирование на Windows

### Использование PowerShell

```powershell
# Создайте тестовый файл
Set-Content -Path "test.txt" -Value "Привет, CryptoCore!"

# Шифрование
.\cryptocore.exe --algorithm aes --mode ecb --encrypt --key 0123456789abcdef0123456789abcdef --input test.txt --output test.enc

# Дешифрование
.\cryptocore.exe --algorithm aes --mode ecb --decrypt --key 0123456789abcdef0123456789abcdef --input test.enc --output test_dec.txt

# Сравните файлы (должны быть идентичны)
fc test.txt test_dec.txt
```

### Использование командной строки

```cmd
REM Создайте тестовый файл
echo Привет, CryptoCore! > test.txt

REM Шифрование
cryptocore.exe --algorithm aes --mode ecb --encrypt --key 0123456789abcdef0123456789abcdef --input test.txt --output test.enc

REM Дешифрование
cryptocore.exe --algorithm aes --mode ecb --decrypt --key 0123456789abcdef0123456789abcdef --input test.enc --output test_dec.txt

REM Сравните файлы
fc test.txt test_dec.txt
```

### Запуск набора тестов

Набор тестов написан на Bash и требует Unix-подобного окружения:

1. **В терминале MSYS2 MINGW64**:
   ```bash
   cd tests
   ./run_tests.sh
   ```

2. **Альтернатива: Ручное тестирование в PowerShell**
   См. примеры выше для ручного тестирования полного цикла.

## Быстрый старт (MSYS2)

```bash
# Установите MSYS2, затем в терминале MSYS2 MINGW64:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-openssl make

# Перейдите в проект
cd /c/school/kripta/3cource

# Соберите
make

# Тестирование
echo "Привет, CryptoCore!" > test.txt
./cryptocore --algorithm aes --mode ecb --encrypt --key 0123456789abcdef0123456789abcdef --input test.txt --output test.enc
./cryptocore --algorithm aes --mode ecb --decrypt --key 0123456789abcdef0123456789abcdef --input test.enc --output test_dec.txt
diff test.txt test_dec.txt
```
