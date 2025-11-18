#!/bin/bash

# ========================================
# Script de Compilación Directa
# Sistema Operativo Descentralizado - Fase 2
# ========================================

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     Compilación Directa - SO Descentralizado              ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# Limpiar compilaciones anteriores
echo -e "${YELLOW}[1/4]${NC} Limpiando archivos anteriores..."
rm -rf bin build logs *.pid
mkdir -p bin build logs src

# Verificar que existe main.c
if [ ! -f "src/main.c" ]; then
    if [ -f "main.c" ]; then
        echo -e "${YELLOW}[!]${NC} Moviendo main.c a src/"
        mv main.c src/main.c
    else
        echo -e "${RED}[✗]${NC} No se encontró main.c"
        echo "    Asegúrate de tener el archivo main.c en el directorio actual o en src/"
        exit 1
    fi
fi

# Compilar
echo -e "${YELLOW}[2/4]${NC} Compilando main.c..."
gcc -Wall -Wextra -O2 -pthread -g -std=c11 -D_GNU_SOURCE \
    -o bin/decentralized_os src/main.c -pthread -lm -lrt

if [ $? -eq 0 ]; then
    echo -e "${GREEN}[✓]${NC} Compilación exitosa"
else
    echo -e "${RED}[✗]${NC} Error en la compilación"
    echo ""
    echo "Intentando compilación con menos opciones..."
    gcc -pthread -o bin/decentralized_os src/main.c -lm
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[✓]${NC} Compilación básica exitosa"
    else
        echo -e "${RED}[✗]${NC} No se pudo compilar el proyecto"
        echo ""
        echo "Posibles soluciones:"
        echo "1. Verifica que tengas gcc instalado: gcc --version"
        echo "2. Instala las herramientas de desarrollo:"
        echo "   Ubuntu/Debian: sudo apt-get install build-essential"
        echo "   Fedora: sudo dnf install gcc make"
        echo "3. Revisa los errores de compilación arriba"
        exit 1
    fi
fi

# Verificar el ejecutable
echo -e "${YELLOW}[3/4]${NC} Verificando ejecutable..."
if [ -f "bin/decentralized_os" ]; then
    echo -e "${GREEN}[✓]${NC} Ejecutable creado: bin/decentralized_os"
    
    # Hacer el ejecutable ejecutable (valga la redundancia)
    chmod +x bin/decentralized_os
    
    # Info del ejecutable
    SIZE=$(ls -lh bin/decentralized_os | awk '{print $5}')
    echo -e "${BLUE}[i]${NC} Tamaño del ejecutable: $SIZE"
else
    echo -e "${RED}[✗]${NC} No se generó el ejecutable"
    exit 1
fi

# Crear scripts de ejecución
echo -e "${YELLOW}[4/4]${NC} Creando scripts de utilidad..."

# Script para ejecutar un nodo
cat > run.sh << 'EOF'
#!/bin/bash
NODE_ID=${1:-0}
echo "Ejecutando nodo $NODE_ID..."
./bin/decentralized_os $NODE_ID
EOF
chmod +x run.sh

# Script para ejecutar cluster
cat > run_cluster.sh << 'EOF'
#!/bin/bash
echo "Iniciando cluster de 3 nodos..."
mkdir -p logs

./bin/decentralized_os 0 > logs/node0.log 2>&1 &
PID0=$!
echo "Nodo 0 iniciado (PID: $PID0)"
sleep 1

./bin/decentralized_os 1 > logs/node1.log 2>&1 &
PID1=$!
echo "Nodo 1 iniciado (PID: $PID1)"
sleep 1

./bin/decentralized_os 2 > logs/node2.log 2>&1 &
PID2=$!
echo "Nodo 2 iniciado (PID: $PID2)"

echo ""
echo "Cluster iniciado. Ver logs en logs/"
echo "PIDs: $PID0, $PID1, $PID2"
echo ""
echo "Para detener: ./stop_cluster.sh o kill $PID0 $PID1 $PID2"
EOF
chmod +x run_cluster.sh

# Script para detener cluster
cat > stop_cluster.sh << 'EOF'
#!/bin/bash
echo "Deteniendo todos los nodos..."
pkill -f decentralized_os
echo "Nodos detenidos"
EOF
chmod +x stop_cluster.sh

# Script para ver logs
cat > view_logs.sh << 'EOF'
#!/bin/bash
if [ -d "logs" ]; then
    echo "Mostrando logs (Ctrl+C para salir)..."
    tail -f logs/node*.log
else
    echo "No hay logs disponibles. Ejecuta primero ./run_cluster.sh"
fi
EOF
chmod +x view_logs.sh

echo -e "${GREEN}[✓]${NC} Scripts creados"

# Resumen final
echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}              ✅ COMPILACIÓN COMPLETADA                     ${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${BLUE}Archivos generados:${NC}"
echo "  📄 bin/decentralized_os - Ejecutable principal"
echo "  📜 run.sh              - Ejecutar un nodo"
echo "  📜 run_cluster.sh      - Ejecutar 3 nodos"
echo "  📜 stop_cluster.sh     - Detener cluster"
echo "  📜 view_logs.sh        - Ver logs"
echo ""
echo -e "${BLUE}Comandos rápidos:${NC}"
echo ""
echo "  Ejecutar un nodo:"
echo -e "    ${YELLOW}./run.sh [ID]${NC}"
echo ""
echo "  Ejecutar cluster de 3 nodos:"
echo -e "    ${YELLOW}./run_cluster.sh${NC}"
echo ""
echo "  Ver logs en tiempo real:"
echo -e "    ${YELLOW}./view_logs.sh${NC}"
echo ""
echo "  Detener todo:"
echo -e "    ${YELLOW}./stop_cluster.sh${NC}"
echo ""
echo -e "${GREEN}¡Listo para usar!${NC}"