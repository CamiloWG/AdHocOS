#!/bin/bash

# Script para verificar la estructura completa del proyecto

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║   Verificación Completa del Proyecto                      ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

missing=0
present=0

check_file() {
    if [ -f "$1" ]; then
        echo -e "${GREEN}✓${NC} $1"
        ((present++))
        return 0
    else
        echo -e "${RED}✗${NC} $1 ${RED}[FALTA]${NC}"
        ((missing++))
        return 1
    fi
}

check_dir() {
    if [ -d "$1" ]; then
        echo -e "${GREEN}✓${NC} Directorio: $1"
        return 0
    else
        echo -e "${YELLOW}⚠${NC}  Directorio no existe: $1"
        return 1
    fi
}

echo -e "${BLUE}=== Verificando Estructura de Directorios ===${NC}"
check_dir "src"
check_dir "src/scheduler"
check_dir "src/memory"
check_dir "src/network"
check_dir "src/sync"
check_dir "src/fault_tolerance"
check_dir "src/ml"
echo ""

echo -e "${BLUE}=== Archivos Raíz ===${NC}"
check_file "Makefile"
check_file "build.sh"
echo ""

echo -e "${BLUE}=== Archivos Principales ===${NC}"
check_file "src/common.h"
check_file "src/main.c"
echo ""

echo -e "${BLUE}=== Módulo Scheduler ===${NC}"
check_file "src/scheduler/scheduler.h"
check_file "src/scheduler/scheduler.c"
echo ""

echo -e "${BLUE}=== Módulo Memory ===${NC}"
check_file "src/memory/memory_manager.h"
check_file "src/memory/memory_manager.c"
echo ""

echo -e "${BLUE}=== Módulo Network ===${NC}"
check_file "src/network/network.h"
check_file "src/network/network.c"
check_file "src/network/discovery.h"
check_file "src/network/discovery.c"
echo ""

echo -e "${BLUE}=== Módulo Sync ===${NC}"
check_file "src/sync/sync.h"
check_file "src/sync/sync.c"
echo ""

echo -e "${BLUE}=== Módulo Fault Tolerance ===${NC}"
check_file "src/fault_tolerance/fault_manager.h"
check_file "src/fault_tolerance/fault_manager.c"
echo ""

echo -e "${BLUE}=== Módulo ML ===${NC}"
check_file "src/ml/ml_lib.h"
check_file "src/ml/ml_lib.c"
echo ""

echo "═══════════════════════════════════════════════════════════"
echo -e "Archivos presentes: ${GREEN}${present}${NC}"
echo -e "Archivos faltantes: ${RED}${missing}${NC}"
echo "═══════════════════════════════════════════════════════════"
echo ""

if [ $missing -eq 0 ]; then
    echo -e "${GREEN}✅ ¡Todos los archivos están presentes!${NC}"
    echo ""
    echo "Estructura de archivos correcta. Puedes compilar:"
    echo -e "  ${YELLOW}chmod +x build.sh${NC}"
    echo -e "  ${YELLOW}./build.sh${NC}"
    echo ""
    echo "O usar make directamente:"
    echo -e "  ${YELLOW}make all${NC}"
    echo ""
else
    echo -e "${RED}⚠️  Faltan $missing archivo(s)${NC}"
    echo ""
    echo "Crea los archivos faltantes antes de compilar."
    echo ""
fi

# Verificar permisos
echo -e "${BLUE}=== Verificando Permisos ===${NC}"
if [ -f "build.sh" ]; then
    if [ -x "build.sh" ]; then
        echo -e "${GREEN}✓${NC} build.sh es ejecutable"
    else
        echo -e "${YELLOW}⚠${NC}  build.sh no es ejecutable. Ejecuta: chmod +x build.sh"
    fi
fi
echo ""

# Contar líneas de código
echo -e "${BLUE}=== Estadísticas del Proyecto ===${NC}"
if command -v wc &> /dev/null; then
    c_files=$(find src -name "*.c" 2>/dev/null | wc -l)
    h_files=$(find src -name "*.h" 2>/dev/null | wc -l)
    
    if [ -d "src" ]; then
        total_lines=$(find src -name "*.c" -o -name "*.h" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
        echo "Archivos .c: $c_files"
        echo "Archivos .h: $h_files"
        echo "Líneas de código totales: $total_lines"
    fi
fi
echo ""

echo "Verificación completada."