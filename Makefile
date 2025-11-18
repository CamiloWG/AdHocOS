# ========================================
# Makefile para Sistema Operativo Descentralizado
# IMPORTANTE: Las líneas de comandos DEBEN empezar con TAB
# ========================================

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -g
LDFLAGS = -pthread -lm -lrt

# Directorios
SRC_DIR = src
BIN_DIR = bin
BUILD_DIR = build

# Archivos
TARGET = $(BIN_DIR)/decentralized_os

# Regla principal
all: directories $(TARGET)
	@echo "✅ Compilación exitosa!"

# Crear directorios
directories:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR) logs

# Compilar ejecutable
$(TARGET): $(SRC_DIR)/main.c
	@echo "Compilando sistema operativo..."
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC_DIR)/main.c $(LDFLAGS)

# Ejecutar
run: $(TARGET)
	@echo "Ejecutando nodo 0..."
	./$(TARGET) 0

# Limpiar
clean:
	@echo "Limpiando..."
	rm -rf $(BIN_DIR) $(BUILD_DIR) logs *.pid

# Ayuda
help:
	@echo "Comandos disponibles:"
	@echo "  make       - Compilar"
	@echo "  make run   - Ejecutar"
	@echo "  make clean - Limpiar"

.PHONY: all directories run clean help