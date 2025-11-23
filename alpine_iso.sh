#!/bin/bash
# Script MVP - Sistema Operativo Descentralizado
# Usa Alpine Linux como base + tu código de red

set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}"
cat << "EOF"
╔═══════════════════════════════════════════════════════════╗
║   CONSTRUCTOR MVP - SO DESCENTRALIZADO                    ║
║   Basado en Alpine Linux + Red Ad-Hoc                     ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# ========================================
# CONFIGURACIÓN
# ========================================

ALPINE_VERSION="3.19"
ALPINE_ISO="alpine-standard-${ALPINE_VERSION}.0-x86_64.iso"
ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_VERSION}/releases/x86_64/${ALPINE_ISO}"
OUTPUT_ISO="decentralized_os_mvp.iso"

# ========================================
# PASO 1: COMPILAR TU CÓDIGO
# ========================================

echo -e "${YELLOW}[1/6]${NC} Compilando sistema descentralizado..."

if [ ! -f "src/main_network.c" ]; then
    echo -e "${RED}Error: No se encuentra src/main_network.c${NC}"
    exit 1
fi

# Compilar versión estática (necesaria para la ISO)
gcc -static -O2 -pthread \
    -o dos_node \
    src/main_network.c \
    -lm || {
    echo -e "${RED}Error en compilación${NC}"
    exit 1
}

echo -e "${GREEN}  ✓ Compilado: dos_node${NC}"

# ========================================
# PASO 2: DESCARGAR ALPINE (si no existe)
# ========================================

echo -e "${YELLOW}[2/6]${NC} Obteniendo Alpine Linux..."

if [ ! -f "$ALPINE_ISO" ]; then
    echo "  Descargando Alpine Linux (~150MB)..."
    wget -q --show-progress "$ALPINE_URL" || {
        echo -e "${RED}Error descargando Alpine${NC}"
        exit 1
    }
fi

echo -e "${GREEN}  ✓ Alpine Linux listo${NC}"

# ========================================
# PASO 3: EXTRAER Y PREPARAR
# ========================================

echo -e "${YELLOW}[3/6]${NC} Preparando sistema de archivos..."

# Limpiar
rm -rf alpine_work alpine_custom
mkdir alpine_work alpine_custom

# Montar ISO original
sudo mount -o loop "$ALPINE_ISO" alpine_work 2>/dev/null || {
    echo -e "${RED}Error montando ISO (necesitas sudo)${NC}"
    exit 1
}

# Copiar contenido
sudo cp -a alpine_work/* alpine_custom/
sudo umount alpine_work

# Crear directorio para nuestro sistema
sudo mkdir -p alpine_custom/dos/bin
sudo mkdir -p alpine_custom/dos/scripts

# Copiar nuestro binario
sudo cp dos_node alpine_custom/dos/bin/
sudo chmod +x alpine_custom/dos/bin/dos_node

echo -e "${GREEN}  ✓ Sistema preparado${NC}"

# ========================================
# PASO 4: CREAR SCRIPTS DE AUTOARRANQUE
# ========================================

echo -e "${YELLOW}[4/6]${NC} Creando scripts de autoarranque..."

# Script principal de inicio
sudo tee alpine_custom/dos/scripts/start.sh > /dev/null << 'EOFSTART'
#!/bin/sh
# Script de inicio del SO Descentralizado

clear

cat << 'BANNER'
╔═══════════════════════════════════════════════════════════╗
║        SISTEMA OPERATIVO DESCENTRALIZADO v1.0             ║
║              Red Ad-Hoc Automática                        ║
╚═══════════════════════════════════════════════════════════╝
BANNER

echo ""
echo "Iniciando sistema..."
sleep 1

# Configurar todas las interfaces de red
echo ""
echo "[RED] Configurando interfaces..."
for iface in eth0 eth1 wlan0; do
    if [ -e "/sys/class/net/$iface" ]; then
        echo "  → $iface"
        ip link set $iface up 2>/dev/null
        udhcpc -i $iface -n -q 2>/dev/null &
    fi
done

sleep 3

# Mostrar configuración
echo ""
echo "[RED] Configuración actual:"
ip -4 addr show | grep inet | grep -v 127.0.0.1 | awk '{print "  " $NF ": " $2}'

echo ""
echo "[SISTEMA] Puertos utilizados:"
echo "  • 8888/UDP - Descubrimiento de nodos"
echo "  • 8889/TCP - Transferencia de datos"
echo ""

# Iniciar nodo descentralizado
echo "[SISTEMA] Iniciando nodo..."
echo ""

cd /dos/bin
exec ./dos_node
EOFSTART

sudo chmod +x alpine_custom/dos/scripts/start.sh

# Script de autoarranque para Alpine
sudo tee alpine_custom/dos/scripts/autostart > /dev/null << 'EOFAUTO'
# Autostart para Alpine Linux
# Este archivo se ejecuta automáticamente al bootear

# Iniciar sistema descentralizado
/dos/scripts/start.sh
EOFAUTO

sudo chmod +x alpine_custom/dos/scripts/autostart

# Archivo .profile para ejecutar automáticamente
sudo tee alpine_custom/dos/scripts/profile > /dev/null << 'EOFPROFILE'
# Profile personalizado
export PS1="[\u@\h \W]\$ "

# Ejecutar sistema descentralizado si no está corriendo
if ! pgrep -x "dos_node" > /dev/null; then
    /dos/scripts/start.sh
fi
EOFPROFILE

# README
sudo tee alpine_custom/dos/README.txt > /dev/null << 'EOFREADME'
╔═══════════════════════════════════════════════════════════╗
║   SISTEMA OPERATIVO DESCENTRALIZADO - GUÍA RÁPIDA        ║
╚═══════════════════════════════════════════════════════════╝

INICIO:
1. Bootea desde esta ISO (BIOS o UEFI)
2. El sistema se iniciará automáticamente
3. Espera ~10 segundos para descubrimiento de nodos

COMANDOS:
  status  - Ver estado del nodo y red
  nodes   - Listar nodos descubiertos
  task    - Crear tarea distribuida
  help    - Ayuda
  quit    - Salir

RED:
- Usa DHCP automáticamente
- Puertos: 8888 (UDP), 8889 (TCP)
- Todos los nodos deben estar en la misma red local

PRUEBA RÁPIDA CON VMs:
1. Inicia 2-3 VMs con esta ISO
2. Configura red en modo "Bridge" o "NAT Network"
3. Los nodos se descubrirán automáticamente

═══════════════════════════════════════════════════════════
EOFREADME

echo -e "${GREEN}  ✓ Scripts creados${NC}"

# ========================================
# PASO 5: MODIFICAR CONFIGURACIÓN DE BOOT
# ========================================

echo -e "${YELLOW}[5/6]${NC} Configurando arranque automático..."

# Crear archivo de respuestas para Alpine (arranque desatendido)
sudo tee alpine_custom/answers > /dev/null << 'EOFANSWER'
# Respuestas automáticas para Alpine setup
KEYMAPOPTS="us us"
HOSTNAMEOPTS="-n dos-node"
INTERFACESOPTS="auto lo
iface lo inet loopback

auto eth0
iface eth0 inet dhcp
"
DNSOPTS="8.8.8.8"
TIMEZONEOPTS="-z UTC"
PROXYOPTS="none"
APKREPOSOPTS="-1"
SSHDOPTS="-c openssh"
NTPOPTS="-c chrony"
DISKOPTS="none"
EOFANSWER

# Modificar isolinux para arranque automático
if [ -f "alpine_custom/boot/syslinux/syslinux.cfg" ]; then
    sudo sed -i 's/TIMEOUT 30/TIMEOUT 10/g' alpine_custom/boot/syslinux/syslinux.cfg
    sudo sed -i 's/PROMPT 1/PROMPT 0/g' alpine_custom/boot/syslinux/syslinux.cfg
fi

# Añadir script de inicio al boot de Alpine
if [ -f "alpine_custom/boot/grub/grub.cfg" ]; then
    sudo tee alpine_custom/boot/grub/grub.cfg > /dev/null << 'EOFGRUB'
set timeout=3
set default=0

menuentry "Sistema Operativo Descentralizado" {
    linux /boot/vmlinuz-lts modules=loop,squashfs,sd-mod,usb-storage quiet
    initrd /boot/initramfs-lts
}
EOFGRUB
fi

echo -e "${GREEN}  ✓ Boot configurado${NC}"

# ========================================
# PASO 6: GENERAR ISO FINAL
# ========================================

echo -e "${YELLOW}[6/6]${NC} Generando ISO final..."

# Usar xorriso para crear la ISO
if command -v xorriso &> /dev/null; then
    sudo xorriso -as mkisofs \
        -o "$OUTPUT_ISO" \
        -isohybrid-mbr alpine_custom/boot/syslinux/isohdpfx.bin \
        -c boot/syslinux/boot.cat \
        -b boot/syslinux/isolinux.bin \
        -no-emul-boot \
        -boot-load-size 4 \
        -boot-info-table \
        alpine_custom/ 2>&1 | grep -v "Warning"
else
    echo -e "${RED}Necesitas instalar xorriso: sudo apt install xorriso${NC}"
    exit 1
fi

# Limpiar
rm -rf alpine_work
sudo rm -rf alpine_custom

if [ -f "$OUTPUT_ISO" ]; then
    SIZE=$(du -h "$OUTPUT_ISO" | cut -f1)
    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                    ✅ ISO CREADA!                         ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "📀 Archivo: $OUTPUT_ISO ($SIZE)"
    echo ""
    echo "🧪 PROBAR EN QEMU:"
    echo "   qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 1024 -enable-kvm"
    echo ""
    echo "🖥️  PROBAR 2 NODOS (en VMs separadas):"
    echo "   Terminal 1: qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 1024 -netdev user,id=n1 -device e1000,netdev=n1"
    echo "   Terminal 2: qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 1024 -netdev user,id=n2 -device e1000,netdev=n2"
    echo ""
    echo "💾 CREAR USB:"
    echo "   sudo dd if=$OUTPUT_ISO of=/dev/sdX bs=4M status=progress"
    echo ""
    echo "⚠️  IMPORTANTE:"
    echo "   • Los PCs/VMs deben estar en la misma red"
    echo "   • Usa red Bridge en VirtualBox/VMware"
    echo "   • Espera ~10 segundos después del boot"
    echo ""
else
    echo -e "${RED}Error: No se pudo crear la ISO${NC}"
    exit 1
fi