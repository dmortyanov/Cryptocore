# CryptoCore Makefile
# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I. -D__USE_MINGW_ANSI_STDIO=1 -finput-charset=UTF-8 -fexec-charset=UTF-8
LDFLAGS = -lcrypto -lbcrypt

# Директории
SRC_DIR = src
MODES_DIR = $(SRC_DIR)/modes
BUILD_DIR = build

# Целевой исполняемый файл
TARGET = cryptocore

# Исходные файлы
SOURCES = main.c \
          $(SRC_DIR)/ecb.c \
          $(SRC_DIR)/file_io.c \
          $(MODES_DIR)/cbc.c \
          $(MODES_DIR)/cfb.c \
          $(MODES_DIR)/ofb.c \
          $(MODES_DIR)/ctr.c \
          $(MODES_DIR)/utils.c \
          $(SRC_DIR)/mouse_entropy.c \
          $(SRC_DIR)/csprng.c

# Объектные файлы
OBJECTS = $(BUILD_DIR)/main.o \
          $(BUILD_DIR)/ecb.o \
          $(BUILD_DIR)/file_io.o \
          $(BUILD_DIR)/cbc.o \
          $(BUILD_DIR)/cfb.o \
          $(BUILD_DIR)/ofb.o \
          $(BUILD_DIR)/ctr.o \
          $(BUILD_DIR)/utils.o \
          $(BUILD_DIR)/mouse_entropy.o \
          $(BUILD_DIR)/csprng.o

# Цель по умолчанию
all: $(BUILD_DIR) $(TARGET)

# Создание директории сборки
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Сборка исполняемого файла
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Сборка завершена: $(TARGET)"

# Компиляция main.c
$(BUILD_DIR)/main.o: main.c include/ecb.h include/modes.h include/file_io.h
	$(CC) $(CFLAGS) -c main.c -o $(BUILD_DIR)/main.o

# Компиляция ecb.c
$(BUILD_DIR)/ecb.o: $(SRC_DIR)/ecb.c include/ecb.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ecb.c -o $(BUILD_DIR)/ecb.o

# Компиляция file_io.c
$(BUILD_DIR)/file_io.o: $(SRC_DIR)/file_io.c include/file_io.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/file_io.c -o $(BUILD_DIR)/file_io.o

# Компиляция cbc.c
$(BUILD_DIR)/cbc.o: $(MODES_DIR)/cbc.c include/modes.h include/ecb.h
	$(CC) $(CFLAGS) -c $(MODES_DIR)/cbc.c -o $(BUILD_DIR)/cbc.o

# Компиляция cfb.c
$(BUILD_DIR)/cfb.o: $(MODES_DIR)/cfb.c include/modes.h include/ecb.h
	$(CC) $(CFLAGS) -c $(MODES_DIR)/cfb.c -o $(BUILD_DIR)/cfb.o

# Компиляция ofb.c
$(BUILD_DIR)/ofb.o: $(MODES_DIR)/ofb.c include/modes.h include/ecb.h
	$(CC) $(CFLAGS) -c $(MODES_DIR)/ofb.c -o $(BUILD_DIR)/ofb.o

# Компиляция ctr.c
$(BUILD_DIR)/ctr.o: $(MODES_DIR)/ctr.c include/modes.h include/ecb.h
	$(CC) $(CFLAGS) -c $(MODES_DIR)/ctr.c -o $(BUILD_DIR)/ctr.o

# Компиляция utils.c
$(BUILD_DIR)/utils.o: $(MODES_DIR)/utils.c include/modes.h
	$(CC) $(CFLAGS) -c $(MODES_DIR)/utils.c -o $(BUILD_DIR)/utils.o

$(BUILD_DIR)/mouse_entropy.o: src/mouse_entropy.c include/mouse_entropy.h
	$(CC) $(CFLAGS) -c src/mouse_entropy.c -o $(BUILD_DIR)/mouse_entropy.o

$(BUILD_DIR)/csprng.o: src/csprng.c include/csprng.h
	$(CC) $(CFLAGS) -c src/csprng.c -o $(BUILD_DIR)/csprng.o

# Очистка артефактов сборки
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Очистка завершена"

# Установка (копирование в /usr/local/bin или аналогичное)
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "Установлен $(TARGET) в /usr/local/bin/"

# Удаление
uninstall:
	rm -f /usr/local/bin/$(TARGET)
	@echo "Удален $(TARGET)"

# Пересборка
rebuild: clean all

.PHONY: all clean install uninstall rebuild
