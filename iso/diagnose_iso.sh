#!/bin/bash

# ========================================
# Script de Diagnóstico para ISO
# ========================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     Diagnóstico de Problemas de Arranque ISO              ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# 1. Verificar archivos necesarios
echo -e "${YELLOW}[1] Verificando archivos necesarios...${NC}"

check_file() {
    if [ -f "$1" ]; then
        echo -e "  ${GREEN}✓${NC} $1 encontrado"
        return 0
    else
        echo -e "  ${RED}✗${NC} $1 NO encontrado"
        return 1
    fi
}

PROBLEMS=0

check_file "decentralized_os.iso" || PROBLEMS=$((PROBLEMS+1))
check_file "vmlinuz" || {
    echo -e "    ${YELLOW}!${NC} Kernel Linux no encontrado"
    PROBLEMS=$((PROBLEMS+1))
}

# 2. Verificar estructura ISO
echo ""
echo -e "${YELLOW}[2] Verificando estructura ISO...${NC}"

if [ -f "decentralized_os.iso" ]; then
    # Montar ISO temporalmente para inspección
    mkdir -p /tmp/iso_check
    sudo mount -o loop decentralized_os.iso /tmp/iso_check 2>/dev/null
    
    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}✓${NC} ISO montada exitosamente"
        
        # Verificar archivos críticos
        check_file "/tmp/iso_check/boot/vmlinuz" || PROBLEMS=$((PROBLEMS+1))
        check_file "/tmp/iso_check/boot/initramfs.cpio.gz" || PROBLEMS=$((PROBLEMS+1))
        check_file "/tmp/iso_check/boot/grub/grub.cfg" || PROBLEMS=$((PROBLEMS+1))
        
        # Verificar contenido de grub.cfg
        if [ -f "/tmp/iso_check/boot/grub/grub.cfg" ]; then
            echo ""
            echo -e "${BLUE}Contenido de grub.cfg:${NC}"
            cat "/tmp/iso_check/boot/grub/grub.cfg" | head -15
        fi
        
        sudo umount /tmp/iso_check
    else
        echo -e "  ${RED}✗${NC} No se pudo montar la ISO"
        PROBLEMS=$((PROBLEMS+1))
    fi
fi

# 3. Verificar kernel
echo ""
echo -e "${YELLOW}[3] Verificando kernel...${NC}"

if [ -f "vmlinuz" ]; then
    file vmlinuz | grep -i "linux kernel" > /dev/null
    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}✓${NC} vmlinuz es un kernel Linux válido"
        file vmlinuz
    else
        echo -e "  ${RED}✗${NC} vmlinuz NO es un kernel válido"
        PROBLEMS=$((PROBLEMS+1))
    fi
else
    echo -e "  ${RED}✗${NC} vmlinuz no encontrado"
    PROBLEMS=$((PROBLEMS+1))
fi

# 4. Verificar initramfs
echo ""
echo -e "${YELLOW}[4] Verificando initramfs...${NC}"

if [ -f "initramfs.cpio.gz" ]; then
    echo -e "  ${GREEN}✓${NC} initramfs.cpio.gz encontrado"
    
    # Verificar que es un archivo cpio válido
    file initramfs.cpio.gz | grep -i "gzip" > /dev/null
    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}✓${NC} initramfs está comprimido correctamente"
        
        # Extraer y verificar contenido
        mkdir -p /tmp/initramfs_check
        cd /tmp/initramfs_check
        gunzip -c ../../initramfs.cpio.gz | cpio -i 2>/dev/null
        
        if [ -f "init" ]; then
            echo -e "  ${GREEN}✓${NC} Script /init encontrado en initramfs"
        else
            echo -e "  ${RED}✗${NC} Script /init NO encontrado en initramfs"
            PROBLEMS=$((PROBLEMS+1))
        fi
        
        if [ -f "sbin/init" ]; then
            echo -e "  ${GREEN}✓${NC} Binario /sbin/init encontrado"
            file sbin/init
        else
            echo -e "  ${RED}✗${NC} Binario /sbin/init NO encontrado"
            PROBLEMS=$((PROBLEMS+1))
        fi
        
        cd - > /dev/null
        rm -rf /tmp/initramfs_check
    else
        echo -e "  ${RED}✗${NC} initramfs NO está comprimido correctamente"
        PROBLEMS=$((PROBLEMS+1))
    fi
fi

# 5. Resumen
echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
if [ $PROBLEMS -eq 0 ]; then
    echo -e "${GREEN}✓ No se detectaron problemas mayores${NC}"
    echo ""
    echo "El problema puede estar en:"
    echo "  1. Configuración de VirtualBox"
    echo "  2. Kernel incompatible con hardware virtual"
    echo "  3. El binario decentralized_os necesita dependencias"
else
    echo -e "${RED}✗ Se detectaron $PROBLEMS problema(s)${NC}"
    echo ""
    echo "Recomendaciones:"
    echo "  1. Ejecuta: ./fix_iso.sh"
    echo "  2. Recrea la ISO con: ./create_iso_fixed.sh"
fi
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
