@echo off
REM Скрипт сборки CryptoCore для Windows
REM Этот скрипт компилирует проект, используя GCC (MinGW)

echo ========================================
echo   Сборка CryptoCore для Windows
echo ========================================
echo.

REM Проверка доступности gcc
where gcc >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: GCC не найден. Пожалуйста, установите MinGW или MSYS2.
    echo.
    echo Варианты установки:
    echo   1. MSYS2: https://www.msys2.org/
    echo   2. MinGW-w64: https://www.mingw-w64.org/
    echo.
    pause
    exit /b 1
)

REM Создание директории сборки
if not exist build mkdir build

echo Компиляция main.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c main.c -o build\main.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать main.c
    pause
    exit /b 1
)

echo Компиляция src\ecb.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c src\ecb.c -o build\ecb.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать src\ecb.c
    pause
    exit /b 1
)

echo Компиляция src\file_io.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c src\file_io.c -o build\file_io.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать src\file_io.c
    pause
    exit /b 1
)

echo Компиляция src\modes\cbc.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c src\modes\cbc.c -o build\cbc.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать src\modes\cbc.c
    pause
    exit /b 1
)

echo Компиляция src\modes\cfb.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c src\modes\cfb.c -o build\cfb.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать src\modes\cfb.c
    pause
    exit /b 1
)

echo Компиляция src\modes\ofb.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c src\modes\ofb.c -o build\ofb.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать src\modes\ofb.c
    pause
    exit /b 1
)

echo Компиляция src\modes\ctr.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c src\modes\ctr.c -o build\ctr.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать src\modes\ctr.c
    pause
    exit /b 1
)

echo Компиляция src\modes\utils.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c src\modes\utils.c -o build\utils.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать src\modes\utils.c
    pause
    exit /b 1
)

echo Компиляция src\mouse_entropy.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c src\mouse_entropy.c -o build\mouse_entropy.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать src\mouse_entropy.c
    pause
    exit /b 1
)

echo Компиляция src\csprng.c...
gcc -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8 -c src\csprng.c -o build\csprng.o
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось скомпилировать src\csprng.c
    pause
    exit /b 1
)

echo Линковка...
gcc build\main.o build\ecb.o build\file_io.o build\cbc.o build\cfb.o build\ofb.o build\ctr.o build\utils.o build\mouse_entropy.o build\csprng.o -o cryptocore.exe -lcrypto -lbcrypt
if %ERRORLEVEL% NEQ 0 (
    echo Ошибка: Не удалось выполнить линковку. Убедитесь, что OpenSSL установлен.
    echo.
    echo Для MSYS2 установите OpenSSL с помощью:
    echo   pacman -S mingw-w64-x86_64-openssl
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Сборка завершена!
echo ========================================
echo.
echo Исполняемый файл: cryptocore.exe
echo.
echo Для тестирования запустите:
echo   cryptocore.exe --help
echo.
pause
