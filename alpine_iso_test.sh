#!/bin/bash
# SCRIPT TODO-EN-UNO - No pienses, solo ejecuta esto
# Compila, crea ISO y prueba en QEMU automáticamente

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

clear
echo -e "${GREEN}${BOLD}"
cat << "EOF"
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║        CONSTRUCTOR AUTOMÁTICO DE ISO                      ║
║        Sistema Operativo Descentralizado                  ║
║                                                           ║
║        Este script hace TODO por ti                       ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

sleep 1

# ========================================
# VERIFICACIONES INICIALES
# ========================================

echo -e "${YELLOW}[INIT]${NC} Verificando requisitos..."

# Verificar GCC
if ! command -v gcc &> /dev/null; then
    echo -e "${RED}Error: gcc no está instalado${NC}"
    echo "Ejecuta: sudo apt install gcc"
    exit 1
fi

# Verificar que estemos en el directorio correcto
if [ ! -f "src/main_network.c" ]; then
    echo -e "${RED}Error: No estás en el directorio del proyecto${NC}"
    echo "Asegúrate de que existe: src/main_network.c"
    exit 1
fi

echo -e "${GREEN}  ✓ Requisitos OK${NC}"

# ========================================
# LIMPIEZA
# ========================================

echo -e "${YELLOW}[CLEAN]${NC} Limpiando archivos anteriores..."

rm -f dos_system *.iso
rm -rf initramfs iso_root alpine_*

echo -e "${GREEN}  ✓ Limpieza completa${NC}"

# ========================================
# COMPILACIÓN
# ========================================

echo -e "${YELLOW}[BUILD]${NC} Compilando sistema..."

gcc -static -O2 -pthread \
    -o dos_system \
    src/main_network.c \
    -lm || {
    echo -e "${RED}Error en compilación${NC}"
    exit 1
}

SIZE=$(du -h dos_system | cut -f1)
echo -e "${GREEN}  ✓ Compilado (${SIZE})${NC}"

# ========================================
# BUSYBOX
# ========================================

echo -e "${YELLOW}[TOOLS]${NC} Descargando busybox..."

if [ ! -f "busybox" ]; then
    wget -q --show-progress https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox
    chmod +x busybox
else
    echo "  (ya existe, usando cache)"
fi

echo -e "${GREEN}  ✓ Busybox listo${NC}"

# ========================================
# INITRAMFS
# ========================================

echo -e "${YELLOW}[INITRAMFS]${NC} Creando sistema de archivos..."

mkdir -p initramfs/{bin,sbin,etc,proc,sys,dev,root,tmp}

# Copiar busybox y crear enlaces
cp busybox initramfs/bin/
cd initramfs/bin
for cmd in sh cat ls mkdir rm cp mv mount umount ifconfig ip ping wget udhcpc sleep echo; do
    ln -sf busybox $cmd 2>/dev/null
done
cd ../..

# Copiar sistema
cp dos_system initramfs/sbin/

# Crear init
cat > initramfs/init << 'EOFINIT'
#!/bin/sh

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

clear
cat << 'BANNER'
╔═══════════════════════════════════════════════════════════╗
║        SISTEMA OPERATIVO DESCENTRALIZADO v1.0             ║
╚═══════════════════════════════════════════════════════════╝
BANNER

echo ""
echo "[RED] Configurando interfaces..."
ifconfig lo 127.0.0.1 up

for iface in eth0 eth1 wlan0 enp0s3; do
    if [ -e "/sys/class/net/$iface" ]; then
        echo "  → $iface"
        ifconfig $iface up 2>/dev/null
        udhcpc -i $iface -n -q 2>/dev/null &
    fi
done

sleep 3

echo ""
echo "[RED] Configuración:"
ifconfig | grep -A1 "eth0\|wlan0" | grep "inet addr" | awk '{print "  " $2}' | sed 's/addr://' || echo "  (Sin IP aún)"

echo ""
echo "[SISTEMA] Puertos: 8888/UDP, 8889/TCP"
echo ""
sleep 1

echo "[SISTEMA] Iniciando nodo descentralizado..."
echo ""

/sbin/dos_system

echo ""
echo "Sistema detenido. Shell:"
/bin/sh
EOFINIT

chmod +x initramfs/init

# Empaquetar
cd initramfs
find . -print0 | cpio --null -o --format=newc 2>/dev/null | gzip -9 > ../initramfs.cpio.gz
cd ..

echo -e "${GREEN}  ✓ Initramfs creado${NC}"

# ========================================
# KERNEL
# ========================================

echo -e "${YELLOW}[KERNEL]${NC} Buscando kernel..."

KERNEL_FOUND=0

if [ -f "/boot/vmlinuz-$(uname -r)" ]; then
    cp "/boot/vmlinuz-$(uname -r)" vmlinuz
    KERNEL_FOUND=1
elif [ -f "/boot/vmlinuz" ]; then
    cp /boot/vmlinuz vmlinuz
    KERNEL_FOUND=1
else
    KERNEL=$(ls /boot/vmlinuz-* 2>/dev/null | head -1)
    if [ -n "$KERNEL" ]; then
        cp "$KERNEL" vmlinuz
        KERNEL_FOUND=1
    fi
fi

if [ $KERNEL_FOUND -eq 0 ]; then
    echo -e "${RED}No se encontró kernel${NC}"
    echo "Instala: sudo apt install linux-image-generic"
    exit 1
fi

echo -e "${GREEN}  ✓ Kernel copiado${NC}"

# ========================================
# ISO
# ========================================

echo -e "${YELLOW}[ISO]${NC} Generando imagen..."

mkdir -p iso_root/boot/grub

cp vmlinuz iso_root/boot/
cp initramfs.cpio.gz iso_root/boot/

cat > iso_root/boot/grub/grub.cfg << 'EOFGRUB'
set timeout=2
set default=0

menuentry "Sistema Operativo Descentralizado" {
    linux /boot/vmlinuz quiet
    initrd /boot/initramfs.cpio.gz
}
EOFGRUB

OUTPUT_ISO="dos_final.iso"

if command -v grub-mkrescue &> /dev/null; then
    grub-mkrescue -o "$OUTPUT_ISO" iso_root/ 2>&1 | grep -v "warning"
elif command -v xorriso &> /dev/null; then
    xorriso -as mkisofs -o "$OUTPUT_ISO" iso_root/ 2>&1 | grep -v "warning"
else
    echo -e "${RED}Necesitas grub-mkrescue o xorriso${NC}"
    exit 1
fi

if [ ! -f "$OUTPUT_ISO" ]; then
    echo -e "${RED}Error creando ISO${NC}"
    exit 1
fi

SIZE=$(du -h "$OUTPUT_ISO" | cut -f1)
echo -e "${GREEN}  ✓ ISO creada (${SIZE})${NC}"

# ========================================
# RESUMEN
# ========================================

echo ""
echo -e "${GREEN}${BOLD}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}${BOLD}║                  ✅ TODO LISTO                           ║${NC}"
echo -e "${GREEN}${BOLD}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}📀 Archivo creado:${NC} $OUTPUT_ISO ($SIZE)"
echo ""
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo -e "${BOLD}OPCIONES:${NC}"
echo ""
echo -e "${GREEN}1.${NC} Probar en QEMU (Opción rápida):"
echo "   ${BLUE}qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 1024 -enable-kvm${NC}"
echo ""
echo -e "${GREEN}2.${NC} Probar en VirtualBox (Recomendado para red):"
echo "   - Crear VM nueva"
echo "   - Montar $OUTPUT_ISO"
echo "   - Red en modo Bridge"
echo "   - Clonar para tener 2+ nodos"
echo ""
echo -e "${GREEN}3.${NC} Crear USB booteable:"
echo "   ${BLUE}sudo dd if=$OUTPUT_ISO of=/dev/sdX bs=4M status=progress${NC}"
echo ""
echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# ========================================
# OPCIÓN DE PRUEBA AUTOMÁTICA
# ========================================

echo -ne "${BOLD}¿Quieres probar en QEMU ahora? (s/N):${NC} "
read -r response

if [[ "$response" =~ ^[Ss]$ ]]; then
    echo ""
    echo -e "${GREEN}Iniciando QEMU...${NC}"
    echo -e "${YELLOW}(Presiona Ctrl+Alt+G para salir del modo captura)${NC}"
    sleep 2
    
    qemu-system-x86_64 \
        -cdrom "$OUTPUT_ISO" \
        -m 1024 \
        -enable-kvm 2>/dev/null || \
    qemu-system-x86_64 \
        -cdrom "$OUTPUT_ISO" \
        -m 1024
else
    echo ""
    echo -e "${GREEN}OK. Ejecuta cuando quieras:${NC}"
    echo "   qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 1024 -enable-kvm"
    echo ""
fi

echo -e "${GREEN}¡Listo! 🚀${NC}"
echo ""