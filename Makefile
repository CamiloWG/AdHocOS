# ========================================
# Makefile Completo - SO Descentralizado
# Con Red REAL y generación de ISO
# ========================================

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -g -std=c11 -D_GNU_SOURCE
LDFLAGS = -pthread -lm -lrt
STATIC_FLAGS = -static -pthread -lm -lrt

# Directorios
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
ISO_DIR = iso_root

# Archivos fuente
MAIN_NETWORK = $(SRC_DIR)/main_network.c
MAIN_SIMPLE = $(SRC_DIR)/main.c

# Ejecutables
TARGET_NETWORK = $(BIN_DIR)/dos_network
TARGET_STATIC = $(BIN_DIR)/dos_static
ISO_FILE = decentralized_os.iso

# ========================================
# Objetivos principales
# ========================================

.PHONY: all network iso clean run test-local test-network install help

# Compilar todo
all: network

# ========================================
# COMPILACIÓN CON RED REAL
# ========================================

network: directories $(TARGET_NETWORK)
	@echo "✅ Sistema con red real compilado"
	@echo "Ejecuta 'make run' para iniciar un nodo"

$(TARGET_NETWORK): $(MAIN_NETWORK)
	@echo "🔨 Compilando sistema con red real..."
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "✅ Ejecutable creado: $@"

# Versión estática para ISO
static: directories
	@echo "🔨 Compilando versión estática para ISO..."
	$(CC) $(CFLAGS) -static -o $(TARGET_STATIC) $(MAIN_NETWORK) $(STATIC_FLAGS)
	@echo "✅ Ejecutable estático creado"

# ========================================
# CREAR IMAGEN ISO
# ========================================

iso: static
	@echo "📀 Creando imagen ISO..."
	@chmod +x create_iso.sh
	@./create_iso.sh
	@echo "✅ ISO creada: $(ISO_FILE)"

# ISO rápida (sin recompilar)
iso-quick:
	@echo "📀 Creando ISO con binarios existentes..."
	@./create_iso.sh
	@echo "✅ ISO creada: $(ISO_FILE)"

# ========================================
# EJECUCIÓN LOCAL
# ========================================

# Ejecutar un nodo
run: $(TARGET_NETWORK)
	@echo "🚀 Iniciando nodo con red real..."
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "El sistema buscará otros nodos en la red local"
	@echo "Puertos utilizados: 8888 (UDP), 8889 (TCP)"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo ""
	./$(TARGET_NETWORK)

# Ejecutar con ID específico
run-id: $(TARGET_NETWORK)
	@echo "🚀 Iniciando nodo con ID: $(ID)"
	./$(TARGET_NETWORK) $(ID)

# ========================================
# PRUEBAS DE RED LOCAL
# ========================================

# Probar con múltiples instancias locales
test-local: $(TARGET_NETWORK)
	@echo "🧪 Iniciando prueba con 3 nodos locales..."
	@mkdir -p logs
	@echo "Iniciando Nodo 1..."
	@./$(TARGET_NETWORK) 1001 > logs/node1.log 2>&1 &
	@echo "PID: $$!"
	@sleep 2
	@echo "Iniciando Nodo 2..."
	@./$(TARGET_NETWORK) 1002 > logs/node2.log 2>&1 &
	@echo "PID: $$!"
	@sleep 2
	@echo "Iniciando Nodo 3..."
	@./$(TARGET_NETWORK) 1003 > logs/node3.log 2>&1 &
	@echo "PID: $$!"
	@echo ""
	@echo "✅ 3 nodos iniciados - Ver logs/ para salida"
	@echo "Usa 'make stop' para detener"

# Probar en red real con múltiples máquinas
test-network:
	@echo "📡 Instrucciones para prueba en red real:"
	@echo ""
	@echo "1. En Máquina 1:"
	@echo "   make run"
	@echo ""
	@echo "2. En Máquina 2:"
	@echo "   make run"
	@echo ""
	@echo "3. En Máquina 3:"
	@echo "   make run"
	@echo ""
	@echo "Los nodos se descubrirán automáticamente."
	@echo "Asegúrate de que:"
	@echo "  - Todas las máquinas están en la misma red"
	@echo "  - El firewall permite puertos 8888-8889"
	@echo "  - No hay conflictos con otros servicios"

# ========================================
# MÁQUINAS VIRTUALES
# ========================================

# Probar ISO en QEMU
test-qemu: iso
	@echo "🖥️  Iniciando QEMU con ISO..."
	qemu-system-x86_64 \
		-cdrom $(ISO_FILE) \
		-m 1024 \
		-netdev user,id=net0,hostfwd=tcp::8889-:8889,hostfwd=udp::8888-:8888 \
		-device e1000,netdev=net0

# Crear cluster de VMs
test-vms: iso
	@echo "🖥️  Creando cluster de 3 VMs..."
	@./test_network_vms.sh

# ========================================
# INSTALACIÓN EN SISTEMA
# ========================================

install: $(TARGET_NETWORK)
	@echo "📦 Instalando en el sistema..."
	@sudo mkdir -p /opt/decentralized_os
	@sudo cp $(TARGET_NETWORK) /opt/decentralized_os/
	@sudo ln -sf /opt/decentralized_os/dos_network /usr/local/bin/dos
	@echo "✅ Instalado. Usa 'dos' para ejecutar"

uninstall:
	@echo "🗑️  Desinstalando..."
	@sudo rm -rf /opt/decentralized_os
	@sudo rm -f /usr/local/bin/dos
	@echo "✅ Desinstalado"

# ========================================
# UTILIDADES
# ========================================

# Ver logs
logs:
	@tail -f logs/*.log

# Detener todos los nodos
stop:
	@echo "⛔ Deteniendo todos los nodos..."
	@pkill -f dos_network || true
	@pkill -f decentralized_os || true
	@echo "✅ Nodos detenidos"

# Verificar red
check-network:
	@echo "🔍 Verificando configuración de red..."
	@echo ""
	@echo "Interfaces de red:"
	@ip addr show | grep -E "^[0-9]+:|inet "
	@echo ""
	@echo "Puertos en uso:"
	@netstat -tuln | grep -E "8888|8889" || echo "Puertos libres ✓"
	@echo ""
	@echo "Firewall (iptables):"
	@sudo iptables -L INPUT -n | grep -E "8888|8889" || echo "Sin reglas específicas"

# Abrir puertos en firewall
open-ports:
	@echo "🔓 Abriendo puertos en firewall..."
	@sudo iptables -I INPUT -p udp --dport 8888 -j ACCEPT
	@sudo iptables -I INPUT -p tcp --dport 8889 -j ACCEPT
	@echo "✅ Puertos 8888-8889 abiertos"

# ========================================
# LIMPIEZA
# ========================================

clean:
	@echo "🧹 Limpiando archivos..."
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(ISO_DIR) logs/
	rm -f $(ISO_FILE) initramfs.cpio.gz vmlinuz
	rm -f *.pid *.log core
	@echo "✅ Limpieza completa"

clean-iso:
	@echo "🧹 Limpiando archivos ISO..."
	rm -rf $(ISO_DIR) $(ISO_FILE) initramfs*
	@echo "✅ Archivos ISO eliminados"

# ========================================
# DIRECTORIOS
# ========================================

directories:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR) $(SRC_DIR) logs

# ========================================
# AYUDA
# ========================================

help:
	@echo "╔═══════════════════════════════════════════════════════════╗"
	@echo "║     SISTEMA OPERATIVO DESCENTRALIZADO - AYUDA             ║"
	@echo "╚═══════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "📡 COMPILACIÓN Y EJECUCIÓN CON RED REAL:"
	@echo "  make network     - Compilar con soporte de red real"
	@echo "  make run         - Ejecutar un nodo"
	@echo "  make run-id ID=X - Ejecutar con ID específico"
	@echo ""
	@echo "🧪 PRUEBAS:"
	@echo "  make test-local  - Probar con 3 nodos locales"
	@echo "  make test-network- Ver instrucciones para red real"
	@echo "  make test-qemu   - Probar ISO en QEMU"
	@echo "  make test-vms    - Crear cluster de VMs"
	@echo ""
	@echo "📀 CREAR ISO BOOTEABLE:"
	@echo "  make iso         - Crear imagen ISO completa"
	@echo "  make iso-quick   - Crear ISO sin recompilar"
	@echo ""
	@echo "🔧 UTILIDADES:"
	@echo "  make logs        - Ver logs en tiempo real"
	@echo "  make stop        - Detener todos los nodos"
	@echo "  make check-network - Verificar configuración de red"
	@echo "  make open-ports  - Abrir puertos en firewall"
	@echo ""
	@echo "📦 INSTALACIÓN:"
	@echo "  make install     - Instalar en el sistema"
	@echo "  make uninstall   - Desinstalar del sistema"
	@echo ""
	@echo "🧹 LIMPIEZA:"
	@echo "  make clean       - Limpiar todo"
	@echo "  make clean-iso   - Limpiar solo archivos ISO"
	@echo ""
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	@echo "Puertos utilizados: 8888 (UDP Discovery), 8889 (TCP Data)"
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"