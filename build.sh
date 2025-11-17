#!/bin/bash

# ========================================
# Script de Compilación - SO Descentralizado
# ========================================

set -e  # Salir si hay errores

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║   Compilando Sistema Operativo Descentralizado            ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Verificar dependencias
echo "🔍 Verificando dependencias..."

if ! command -v gcc &> /dev/null; then
    echo -e "${RED}❌ gcc no está instalado${NC}"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo -e "${RED}❌ make no está instalado${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Dependencias verificadas${NC}"
echo ""

# Limpiar compilación anterior
echo "🧹 Limpiando archivos anteriores..."
make clean 2>/dev/null || true
echo ""

# Crear directorios necesarios
echo "📁 Creando estructura de directorios..."
mkdir -p logs
mkdir -p bin
echo -e "${GREEN}✅ Directorios creados${NC}"
echo ""

# Compilar
echo "🔨 Compilando proyecto..."
if make all; then
    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║            ✅ COMPILACIÓN EXITOSA ✅                       ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "Opciones disponibles:"
    echo ""
    echo "  1. Ejecutar nodo único (demo):"
    echo "     ${YELLOW}./bin/decentralized_os 0${NC}"
    echo ""
    echo "  2. Ejecutar cluster de 3 nodos:"
    echo "     ${YELLOW}make test-cluster${NC}"
    echo "     (Ver logs en logs/nodeN.log)"
    echo ""
    echo "  3. Ejecutar en modo interactivo:"
    echo "     ${YELLOW}./bin/decentralized_os 0 i${NC}"
    echo ""
    echo "  4. Detener cluster:"
    echo "     ${YELLOW}make stop-cluster${NC}"
    echo ""
else
    echo ""
    echo -e "${RED}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║              ❌ ERROR EN COMPILACIÓN ❌                    ║${NC}"
    echo -e "${RED}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "Revisa los errores arriba y corrige los problemas."
    exit 1
fi