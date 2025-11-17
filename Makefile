# ========================================
# Makefile - Sistema Operativo Descentralizado v2.0
# Arquitectura 64-bit - Fase 2 Completa
# ========================================

CC = gcc
CFLAGS = -Wall -Wextra -O2 -march=native -m64 -pthread -g \
         -mavx2 -mfma -D_GNU_SOURCE -std=gnu11
LDFLAGS = -pthread -lm -lcrypto -lssl -lz -lrt

# Directorios
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INC_DIR = include

# Archivos principales
KERNEL_SRC = kernel_64bit.c
DFS_SRC = dfs.h
SYSCALLS_SRC = distributed_syscalls.h
MAIN_SRC = main_complete.c

# Archivos fuente originales (si existen)
ORIGINAL_SRCS = $(wildcard $(SRC_DIR)/*.c) \
                $(wildcard $(SRC_DIR)/*/*.c)

# Ejecutables
TARGET_MAIN = $(BIN_DIR)/dos_64bit
TARGET_DEMO = $(BIN_DIR)/dos_demo
TARGET_CLUSTER = $(BIN_DIR)/dos_cluster_node

# ========================================
# REGLAS PRINCIPALES
# ========================================

.PHONY: all clean dirs run demo cluster help test benchmark

all: dirs $(TARGET_MAIN) $(TARGET_DEMO)
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════════════╗"
	@echo "║  ✅ COMPILACIÓN EXITOSA - SISTEMA OPERATIVO DESCENTRALIZADO v2.0 ║"
	@echo "╚══════════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "Ejecutables creados:"
	@echo "  - $(TARGET_MAIN): Sistema principal"
	@echo "  - $(TARGET_DEMO): Modo demostración"
	@echo ""
	@echo "Usa 'make help' para ver todas las opciones"

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) logs

# ========================================
# COMPILACIÓN PRINCIPAL
# ========================================

$(TARGET_MAIN): $(MAIN_SRC) $(KERNEL_SRC) $(DFS_SRC) $(SYSCALLS_SRC)
	@echo "Compilando sistema principal (64-bit con AVX2)..."
	$(CC) $(CFLAGS) -o $@ $(MAIN_SRC) $(LDFLAGS)
	@echo "✓ Sistema principal compilado: $@"

$(TARGET_DEMO): $(MAIN_SRC) $(KERNEL_SRC) $(DFS_SRC) $(SYSCALLS_SRC)
	@echo "Compilando versión demo..."
	$(CC) $(CFLAGS) -DDEMO_MODE -o $@ $(MAIN_SRC) $(LDFLAGS)
	@echo "✓ Versión demo compilada: $@"

$(TARGET_CLUSTER): $(MAIN_SRC) $(KERNEL_SRC) $(DFS_SRC) $(SYSCALLS_SRC)
	@echo "Compilando nodo de cluster..."
	$(CC) $(CFLAGS) -DCLUSTER_MODE -o $@ $(MAIN_SRC) $(LDFLAGS)
	@echo "✓ Nodo de cluster compilado: $@"

# ========================================
# EJECUCIÓN Y PRUEBAS
# ========================================

run: $(TARGET_MAIN)
	@echo "🚀 Ejecutando Sistema Operativo Descentralizado (Nodo 0)..."
	@./$(TARGET_MAIN) 0

demo: $(TARGET_DEMO)
	@echo "🎮 Ejecutando todas las demos..."
	@./$(TARGET_DEMO) 0 demo

demo1: $(TARGET_MAIN)
	@echo "Demo 1: Procesos Distribuidos"
	@echo "demo 1" | ./$(TARGET_MAIN) 0

demo2: $(TARGET_MAIN)
	@echo "Demo 2: Sistema de Archivos Distribuido"
	@echo "demo 2" | ./$(TARGET_MAIN) 0

demo3: $(TARGET_MAIN)
	@echo "Demo 3: Memoria Compartida Distribuida"
	@echo "demo 3" | ./$(TARGET_MAIN) 0

demo4: $(TARGET_MAIN)
	@echo "Demo 4: Machine Learning Distribuido"
	@echo "demo 4" | ./$(TARGET_MAIN) 0

demo5: $(TARGET_MAIN)
	@echo "Demo 5: Tolerancia a Fallos"
	@echo "demo 5" | ./$(TARGET_MAIN) 0

# ========================================
# CLUSTER DE PRUEBA
# ========================================

cluster: $(TARGET_MAIN)
	@echo "🌐 Iniciando cluster de 3 nodos..."
	@mkdir -p logs
	@./$(TARGET_MAIN) 0 > logs/node0.log 2>&1 & echo $$! > cluster.pids
	@sleep 1
	@./$(TARGET_MAIN) 1 > logs/node1.log 2>&1 & echo $$! >> cluster.pids
	@sleep 1
	@./$(TARGET_MAIN) 2 > logs/node2.log 2>&1 & echo $$! >> cluster.pids
	@echo "✅ Cluster iniciado con 3 nodos"
	@echo "   Logs en: logs/node{0,1,2}.log"
	@echo "   Detener con: make stop-cluster"

stop-cluster:
	@echo "🛑 Deteniendo cluster..."
	@if [ -f cluster.pids ]; then \
		while read pid; do \
			kill -TERM $$pid 2>/dev/null || true; \
		done < cluster.pids; \
		rm cluster.pids; \
		echo "✅ Cluster detenido"; \
	else \
		echo "⚠️  No hay cluster en ejecución"; \
	fi

monitor:
	@echo "📊 Monitoreando cluster..."
	@tail -f logs/node*.log

# ========================================
# PRUEBAS Y BENCHMARKS
# ========================================

test: $(TARGET_MAIN)
	@echo "🧪 Ejecutando pruebas del sistema..."
	@echo ""
	@echo "=== Test 1: Creación de procesos ==="
	@./$(TARGET_MAIN) 0 test processes
	@echo ""
	@echo "=== Test 2: Sistema de archivos ==="
	@./$(TARGET_MAIN) 0 test filesystem
	@echo ""
	@echo "=== Test 3: Memoria compartida ==="
	@./$(TARGET_MAIN) 0 test memory
	@echo ""
	@echo "=== Test 4: SIMD/AVX2 ==="
	@./$(TARGET_MAIN) 0 test simd
	@echo ""
	@echo "✅ Todas las pruebas completadas"

benchmark: $(TARGET_MAIN)
	@echo "⚡ Ejecutando benchmarks..."
	@echo ""
	@echo "=== Benchmark: Producto punto (1M elementos) ==="
	@./$(TARGET_MAIN) 0 benchmark dot-product
	@echo ""
	@echo "=== Benchmark: Escritura/Lectura (100MB) ==="
	@./$(TARGET_MAIN) 0 benchmark io
	@echo ""
	@echo "=== Benchmark: Fork distribuido (1000 procesos) ==="
	@./$(TARGET_MAIN) 0 benchmark fork
	@echo ""
	@echo "=== Benchmark: Entrenamiento ML (10k muestras) ==="
	@./$(TARGET_MAIN) 0 benchmark ml
	@echo ""
	@echo "✅ Benchmarks completados"

# ========================================
# VERIFICACIÓN DEL SISTEMA
# ========================================

check-cpu:
	@echo "🔍 Verificando capacidades del CPU..."
	@echo ""
	@lscpu | grep -E "Model name|CPU\(s\)|Thread|Core|Socket|MHz|Cache"
	@echo ""
	@echo "Flags AVX/AVX2:"
	@cat /proc/cpuinfo | grep -m1 flags | grep -o -E "avx2?|fma|sse[0-9]" | tr '\n' ' '
	@echo ""
	@echo ""
	@if cat /proc/cpuinfo | grep -q avx2; then \
		echo "✅ AVX2 soportado - Optimizaciones SIMD disponibles"; \
	else \
		echo "⚠️  AVX2 no detectado - Rendimiento reducido"; \
	fi

check-deps:
	@echo "🔍 Verificando dependencias..."
	@command -v gcc >/dev/null 2>&1 && echo "✓ GCC instalado: $$(gcc --version | head -1)" || echo "✗ GCC no encontrado"
	@command -v make >/dev/null 2>&1 && echo "✓ Make instalado: $$(make --version | head -1)" || echo "✗ Make no encontrado"
	@echo "✓ Threads: $$(getconf _NPROCESSORS_ONLN) disponibles"
	@echo "✓ Memoria: $$(free -h | grep Mem | awk '{print $$2}') total"
	@pkg-config --exists openssl && echo "✓ OpenSSL instalado" || echo "⚠️  OpenSSL no encontrado (opcional)"
	@pkg-config --exists zlib && echo "✓ zlib instalado" || echo "⚠️  zlib no encontrado (opcional)"

# ========================================
# INSTALACIÓN
# ========================================

install: all
	@echo "📦 Instalando sistema..."
	@mkdir -p /usr/local/bin
	@cp $(TARGET_MAIN) /usr/local/bin/dos64
	@echo "✅ Instalado en /usr/local/bin/dos64"

uninstall:
	@echo "🗑️  Desinstalando..."
	@rm -f /usr/local/bin/dos64
	@echo "✅ Desinstalación completa"

# ========================================
# DOCUMENTACIÓN
# ========================================

docs:
	@echo "📚 Generando documentación..."
	@mkdir -p docs
	@echo "# Sistema Operativo Descentralizado v2.0" > docs/README.md
	@echo "" >> docs/README.md
	@echo "## Arquitectura 64-bit con optimizaciones AVX2" >> docs/README.md
	@echo "" >> docs/README.md
	@echo "### Características principales:" >> docs/README.md
	@echo "- Kernel distribuido de 64 bits" >> docs/README.md
	@echo "- Sistema de archivos distribuido (DFS)" >> docs/README.md
	@echo "- Memoria compartida con locks R/W" >> docs/README.md
	@echo "- Syscalls distribuidas" >> docs/README.md
	@echo "- Machine Learning con SIMD" >> docs/README.md
	@echo "- Tolerancia a fallos" >> docs/README.md
	@echo "" >> docs/README.md
	@echo "### Compilación:" >> docs/README.md
	@echo '```bash' >> docs/README.md
	@echo "make all      # Compilar todo" >> docs/README.md
	@echo "make demo     # Ejecutar demos" >> docs/README.md
	@echo "make cluster  # Iniciar cluster" >> docs/README.md
	@echo '```' >> docs/README.md
	@echo "✅ Documentación generada en docs/"

# ========================================
# LIMPIEZA
# ========================================

clean:
	@echo "🧹 Limpiando archivos..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR) logs/ cluster.pids *.o *.log
	@echo "✅ Limpieza completa"

distclean: clean
	@rm -rf docs/
	@echo "✅ Limpieza profunda completada"

# ========================================
# AYUDA
# ========================================

help:
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════════════╗"
	@echo "║  SISTEMA OPERATIVO DESCENTRALIZADO v2.0 - AYUDA                 ║"
	@echo "╚══════════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "📦 COMPILACIÓN:"
	@echo "  make all          - Compilar todo el sistema"
	@echo "  make clean        - Limpiar archivos compilados"
	@echo ""
	@echo "🚀 EJECUCIÓN:"
	@echo "  make run          - Ejecutar nodo único"
	@echo "  make demo         - Ejecutar todas las demos"
	@echo "  make demo1-5      - Ejecutar demo específica"
	@echo ""
	@echo "🌐 CLUSTER:"
	@echo "  make cluster      - Iniciar cluster de 3 nodos"
	@echo "  make stop-cluster - Detener cluster"
	@echo "  make monitor      - Monitorear logs del cluster"
	@echo ""
	@echo "🧪 PRUEBAS:"
	@echo "  make test         - Ejecutar suite de pruebas"
	@echo "  make benchmark    - Ejecutar benchmarks"
	@echo ""
	@echo "🔍 VERIFICACIÓN:"
	@echo "  make check-cpu    - Verificar capacidades del CPU"
	@echo "  make check-deps   - Verificar dependencias"
	@echo ""
	@echo "📚 OTROS:"
	@echo "  make docs         - Generar documentación"
	@echo "  make install      - Instalar en el sistema"
	@echo "  make help         - Mostrar esta ayuda"
	@echo ""

# ========================================
# REGLAS ESPECIALES
# ========================================

.PRECIOUS: $(BUILD_DIR)/%.o
.SECONDARY:

# Compilación con diferentes niveles de optimización
debug: CFLAGS += -O0 -DDEBUG -g3 -fsanitize=address
debug: all

release: CFLAGS += -O3 -march=native -flto -DNDEBUG
release: LDFLAGS += -flto
release: all

# Profiling
profile: CFLAGS += -pg -O2
profile: LDFLAGS += -pg
profile: all
	@echo "✅ Compilado con profiling. Usa gprof para analizar"
