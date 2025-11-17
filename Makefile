# ========================================
# Makefile para Sistema Operativo Descentralizado
# ========================================

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -g -I$(SRC_DIR)
LDFLAGS = -pthread -lm

# Directorios
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

# Archivos fuente principales
COMMON_SRC = $(SRC_DIR)/common.c
SCHEDULER_SRC = $(SRC_DIR)/scheduler/scheduler.c
MEMORY_SRC = $(SRC_DIR)/memory/memory_manager.c
NETWORK_SRC = $(SRC_DIR)/network/network.c $(SRC_DIR)/network/discovery.c
SYNC_SRC = $(SRC_DIR)/sync/sync.c
FAULT_SRC = $(SRC_DIR)/fault_tolerance/fault_manager.c
ML_SRC = $(SRC_DIR)/ml/ml_lib.c
MAIN_SRC = $(SRC_DIR)/main.c

# Todos los archivos fuente
ALL_SRCS = $(COMMON_SRC) $(MAIN_SRC) $(SCHEDULER_SRC) $(MEMORY_SRC) \
           $(NETWORK_SRC) $(SYNC_SRC) $(FAULT_SRC) $(ML_SRC)

# Archivos objeto
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ALL_SRCS))

# Ejecutable
TARGET = $(BIN_DIR)/decentralized_os

# ========================================
# Reglas principales
# ========================================

all: directories $(TARGET)
	@echo "✅ Compilación exitosa!"
	@echo "Ejecuta: make run        - Para un solo nodo"
	@echo "         make test-cluster - Para cluster de 3 nodos"

directories:
	@mkdir -p $(BUILD_DIR)/scheduler $(BUILD_DIR)/memory
	@mkdir -p $(BUILD_DIR)/network $(BUILD_DIR)/sync $(BUILD_DIR)/fault_tolerance
	@mkdir -p $(BUILD_DIR)/ml $(BIN_DIR) logs

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)
	@echo "✅ Kernel compilado: $(TARGET)"

# Compilar archivos individuales
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compilado: $<"

# ========================================
# Ejecución y pruebas
# ========================================

run: $(TARGET)
	@echo "🚀 Ejecutando nodo único..."
	./$(TARGET) 0

test-cluster: directories $(TARGET)
	@echo "🚀 Iniciando cluster de 3 nodos..."
	@mkdir -p logs
	@./$(TARGET) 0 > logs/node0.log 2>&1 & echo $$! > cluster.pids
	@sleep 1
	@./$(TARGET) 1 > logs/node1.log 2>&1 & echo $$! >> cluster.pids
	@sleep 1
	@./$(TARGET) 2 > logs/node2.log 2>&1 & echo $$! >> cluster.pids
	@echo "✅ Cluster iniciado. Ver logs en logs/"
	@echo "   Para detener: make stop-cluster"

stop-cluster:
	@echo "🛑 Deteniendo cluster..."
	@if [ -f cluster.pids ]; then \
		while read pid; do kill -9 $$pid 2>/dev/null || true; done < cluster.pids; \
		rm cluster.pids; \
		echo "✅ Cluster detenido"; \
	else \
		echo "⚠️  No hay cluster en ejecución"; \
	fi

logs:
	@mkdir -p logs
	@tail -f logs/node*.log

# ========================================
# Limpieza
# ========================================

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) logs/ cluster.pids
	@echo "✅ Limpieza completa"

# ========================================
# Información y debug
# ========================================

info:
	@echo "=== Información del Proyecto ==="
	@echo "Archivos fuente: $(words $(ALL_SRCS))"
	@echo "Archivos objeto: $(words $(OBJS))"
	@echo "Compilador: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo ""
	@echo "Archivos a compilar:"
	@for src in $(ALL_SRCS); do echo "  - $$src"; done

debug: CFLAGS += -DDEBUG -g3
debug: clean all
	@echo "✅ Compilado en modo debug"

.PHONY: all clean directories run test-cluster stop-cluster logs info debug