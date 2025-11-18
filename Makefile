# ========================================
# Makefile para Sistema Operativo Descentralizado
# ========================================

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -g
LDFLAGS = -pthread -lm

# Directorios
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
KERNEL_DIR = $(SRC_DIR)/kernel
NET_DIR = $(SRC_DIR)/network
SCHED_DIR = $(SRC_DIR)/scheduler
MEM_DIR = $(SRC_DIR)/memory

# Archivos fuente
KERNEL_SRCS = kernel.c
SCHEDULER_SRCS = scheduler.c
MEMORY_SRCS = memory_manager.c
NETWORK_SRCS = network.c discovery.c
ML_SRCS = ml_lib.c

# Objetivos
TARGET = decentralized_os
BOOTLOADER = bootloader

# ========================================
# Reglas de compilación
# ========================================

all: directories $(TARGET) bootloader image

directories:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

$(TARGET): $(BUILD_DIR)/main.o
	$(CC) $(LDFLAGS) -o $(BIN_DIR)/$(TARGET) $(BUILD_DIR)/*.o
	@echo "✅ Kernel compilado exitosamente"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ========================================
# Bootloader (para arranque desde hardware real)
# ========================================

bootloader:
	@echo "Compilando bootloader..."
	nasm -f bin boot/bootloader.asm -o $(BIN_DIR)/bootloader.bin
	@echo "✅ Bootloader creado"

# ========================================
# Crear imagen del SO
# ========================================

image: $(TARGET) bootloader
	@echo "Creando imagen del sistema operativo..."
	dd if=/dev/zero of=$(BIN_DIR)/os.img bs=1M count=32
	dd if=$(BIN_DIR)/bootloader.bin of=$(BIN_DIR)/os.img conv=notrunc
	dd if=$(BIN_DIR)/$(TARGET) of=$(BIN_DIR)/os.img seek=1 conv=notrunc
	@echo "✅ Imagen os.img creada (32MB)"

# ========================================
# Imagen ISO para máquinas virtuales
# ========================================

iso: image
	@echo "Creando imagen ISO..."
	mkdir -p iso/boot/grub
	cp $(BIN_DIR)/$(TARGET) iso/boot/
	echo 'menuentry "Decentralized OS" {' > iso/boot/grub/grub.cfg
	echo '    multiboot /boot/$(TARGET)' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BIN_DIR)/decentralized_os.iso iso/
	@echo "✅ ISO creado: decentralized_os.iso"

# ========================================
# Testing y debugging
# ========================================

run: $(TARGET)
	@echo "Ejecutando sistema operativo..."
	./$(BIN_DIR)/$(TARGET)

run-qemu: image
	@echo "Ejecutando en QEMU..."
	qemu-system-x86_64 -drive format=raw,file=$(BIN_DIR)/os.img -m 512M

debug: $(TARGET)
	gdb ./$(BIN_DIR)/$(TARGET)

# ========================================
# Testing distribuido (múltiples nodos)
# ========================================

test-cluster:
	@echo "Iniciando cluster de prueba con 3 nodos..."
	@./$(BIN_DIR)/$(TARGET) 0 &
	@sleep 1
	@./$(BIN_DIR)/$(TARGET) 1 &
	@sleep 1
	@./$(BIN_DIR)/$(TARGET) 2 &
	@echo "✅ Cluster iniciado - PIDs guardados en cluster.pids"
	@ps aux | grep $(TARGET) | grep -v grep | awk '{print $$2}' > cluster.pids

stop-cluster:
	@echo "Deteniendo cluster..."
	@if [ -f cluster.pids ]; then \
		cat cluster.pids | xargs kill -9 2>/dev/null || true; \
		rm cluster.pids; \
	fi
	@echo "✅ Cluster detenido"

# ========================================
# Limpieza
# ========================================

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) iso/ *.pids
	@echo "✅ Limpieza completa"

.PHONY: all clean directories bootloader image iso run run-qemu debug test-cluster stop-cluster

# ========================================
# Script de compilación rápida (build.sh)
# ========================================
# Crear archivo build.sh con el siguiente contenido:
# #!/bin/bash
# echo "🔨 Compilando Sistema Operativo Descentralizado..."
# make clean
# make all
# if [ $? -eq 0 ]; then
#     echo "✅ Compilación exitosa!"
#     echo "Ejecuta 'make run' para probar el SO"
#     echo "Ejecuta 'make test-cluster' para probar múltiples nodos"
# else
#     echo "❌ Error en la compilación"
#     exit 1
# fi