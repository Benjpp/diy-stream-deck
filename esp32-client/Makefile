# Nota: Este Makefile es para compilación local/estática. 
# Para ESP32 nativo se usa normalmente 'idf.py build' (CMake), 
# pero creamos este para gestión de archivos o pruebas rápidas.

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

TARGET = esp32client.o
SRC = src/esp32client.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
