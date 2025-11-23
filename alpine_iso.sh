#!/bin/bash
# Script CORREGIDO - Integra el SO Descentralizado en Alpine Linux
# Este script SÍ ejecuta tu código automáticamente al bootear

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

clear
echo -e "${GREEN}"
cat << "EOF"
╔═══════════════════════════════════════════════════════════╗
║   CONSTRUCTOR CORREGIDO - SO DESCENTRALIZADO             ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# ========================================
# PASO 1: COMPILAR CÓDIGO
# ========================================

echo -e "${YELLOW}[1/7]${NC} Compilando sistema..."

if [ ! -f "src/main_network.c" ]; then
    echo -e "${RED}Error: No se encuentra src/main_network.c${NC}"
    exit 1
fi

gcc -static -O2 -pthread -o dos_node src/main_network.c -lm || {
    echo -e "${RED}Error compilando${NC}"
    exit 1
}

echo -e "${GREEN}  ✓ Compilado${NC}"

# ========================================
# PASO 2: DESCARGAR ALPINE
# ========================================

echo -e "${YELLOW}[2/7]${NC} Descargando Alpine Linux..."

ALPINE_ISO="alpine-standard-3.19.0-x86_64.iso"
ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/x86_64/${ALPINE_ISO}"

if [ ! -f "$ALPINE_ISO" ]; then
    wget -q --show-progress "$ALPINE_URL"
fi

echo -e "${GREEN}  ✓ Alpine listo${NC}"

# ========================================
# PASO 3: EXTRAER ALPINE
# ========================================

echo -e "${YELLOW}[3/7]${NC} Extrayendo Alpine..."

rm -rf alpine_work alpine_custom
mkdir -p alpine_work alpine_custom

sudo mount -o loop "$ALPINE_ISO" alpine_work
sudo cp -a alpine_work/* alpine_custom/
sudo umount alpine_work

echo -e "${GREEN}  ✓ Extraído${NC}"

# ========================================
# PASO 4: MODIFICAR INITRAMFS
# ========================================

echo -e "${YELLOW}[4/7]${NC} Modificando initramfs..."

# Extraer initramfs actual
cd alpine_custom/boot
sudo gunzip -c initramfs-lts > /tmp/initramfs.cpio
cd ../../

# Crear directorio temporal para modificar
mkdir -p initramfs_mod
cd initramfs_mod
sudo cpio -idm < /tmp/initramfs.cpio 2>/dev/null

# Crear directorio para nuestro sistema
sudo mkdir -p dos/bin

# Copiar nuestro binario
sudo cp ../dos_node dos/bin/
sudo chmod +x dos/bin/dos_node

# CRÍTICO: Modificar el script init de Alpine
sudo tee init_wrapper.sh > /dev/null << 'EOFWRAPPER'
#!/bin/sh
# Wrapper para ejecutar el sistema descentralizado

# Montar sistemas básicos
mount -t proc none /proc 2>/dev/null
mount -t sysfs none /sys 2>/dev/null  
mount -t devtmpfs none /dev 2>/dev/null

# Banner
clear
cat << 'BANNER'
╔═══════════════════════════════════════════════════════════╗
║        SISTEMA OPERATIVO DESCENTRALIZADO v1.0             ║
║              Iniciando Red Ad-Hoc...                      ║
╚═══════════════════════════════════════════════════════════╝
BANNER

echo ""
echo "[BOOT] Configurando red..."

# Configurar red
ip link set lo up
for iface in eth0 eth1 wlan0; do
    if [ -e "/sys/class/net/$iface" ]; then
        echo "  → $iface"
        ip link set $iface up
        udhcpc -i $iface -n -q 2>/dev/null &
    fi
done

sleep 3

echo ""
echo "[BOOT] Red configurada:"
ip -4 addr show | grep inet | grep -v 127.0.0.1 | awk '{print "  " $NF ": " $2}'

echo ""
echo "[SISTEMA] Puertos:"
echo "  • 8888/UDP - Discovery"
echo "  • 8889/TCP - Data"
echo ""

# EJECUTAR SISTEMA DESCENTRALIZADO
echo "[SISTEMA] Iniciando nodo..."
cd /dos/bin
exec ./dos_node
EOFWRAPPER

sudo chmod +x init_wrapper.sh

# Respaldar init original
sudo cp init init.original

# Crear nuevo init que llame a nuestro wrapper
sudo tee init > /dev/null << 'EOFINIT'
#!/bin/sh
# Init modificado para SO Descentralizado

# Ejecutar wrapper directamente
exec /init_wrapper.sh
EOFINIT

sudo chmod +x init

# Reempaquetar initramfs
sudo find . | sudo cpio -o -H newc 2>/dev/null | gzip -9 > /tmp/initramfs-custom.gz

# Reemplazar en la ISO
sudo cp /tmp/initramfs-custom.gz ../alpine_custom/boot/initramfs-lts

cd ..
rm -rf initramfs_mod

echo -e "${GREEN}  ✓ Initramfs modificado${NC}"

# ========================================
# PASO 5: CONFIGURAR GRUB
# ========================================

echo -e "${YELLOW}[5/7]${NC} Configurando boot..."

if [ -f "alpine_custom/boot/grub/grub.cfg" ]; then
    sudo tee alpine_custom/boot/grub/grub.cfg > /dev/null << 'EOFGRUB'
set timeout=2
set default=0

menuentry "Sistema Operativo Descentralizado" {
    linux /boot/vmlinuz-lts quiet console=tty0
    initrd /boot/initramfs-lts
}
EOFGRUB
fi

# Configurar syslinux también
if [ -f "alpine_custom/boot/syslinux/syslinux.cfg" ]; then
    sudo sed -i 's/TIMEOUT 30/TIMEOUT 20/g' alpine_custom/boot/syslinux/syslinux.cfg
    sudo sed -i 's/PROMPT 1/PROMPT 0/g' alpine_custom/boot/syslinux/syslinux.cfg
fi

echo -e "${GREEN}  ✓ Boot configurado${NC}"

# ========================================
# PASO 6: AÑADIR DOCUMENTACIÓN
# ========================================

echo -e "${YELLOW}[6/7]${NC} Añadiendo docs..."

sudo tee alpine_custom/README_DOS.txt > /dev/null << 'EOFREADME'
╔═══════════════════════════════════════════════════════════╗
║   SISTEMA OPERATIVO DESCENTRALIZADO                       ║
╚═══════════════════════════════════════════════════════════╝

INICIO:
- Bootea automáticamente
- Configura red con DHCP
- Descubre nodos en ~10 segundos

COMANDOS:
  status  - Estado del sistema
  nodes   - Nodos en la red
  task    - Crear tarea
  help    - Ayuda
  quit    - Salir

RED:
- Puertos: 8888 (UDP), 8889 (TCP)
- Requiere misma red local

PRUEBA:
- Inicia 2+ VMs con esta ISO
- Configura red Bridge
- Los nodos se descubrirán automáticamente
EOFREADME

echo -e "${GREEN}  ✓ Docs añadidos${NC}"

# ========================================
# PASO 7: GENERAR ISO
# ========================================

echo -e "${YELLOW}[7/7]${NC} Generando ISO..."

OUTPUT_ISO="decentralized_os.iso"

sudo xorriso -as mkisofs \
    -o "$OUTPUT_ISO" \
    -isohybrid-mbr alpine_custom/boot/syslinux/isohdpfx.bin \
    -c boot/syslinux/boot.cat \
    -b boot/syslinux/isolinux.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    alpine_custom/ 2>&1 | grep -v "Warning" || {
    echo -e "${RED}Error generando ISO${NC}"
    exit 1
}

# Cambiar permisos
sudo chown $USER:$USER "$OUTPUT_ISO" 2>/dev/null || true

# Limpiar
rm -rf alpine_work
sudo rm -rf alpine_custom

if [ -f "$OUTPUT_ISO" ]; then
    SIZE=$(du -h "$OUTPUT_ISO" | cut -f1)
    
    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                ✅ ISO CREADA CORRECTAMENTE!              ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "📀 Archivo: $OUTPUT_ISO ($SIZE)"
    echo ""
    echo -e "${BLUE}🧪 PROBAR AHORA:${NC}"
    echo "   qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 1024 -enable-kvm"
    echo ""
    echo -e "${BLUE}🔧 PROBAR 2 NODOS:${NC}"
    echo "   Terminal 1: qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 1024"
    echo "   Terminal 2: qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 1024"
    echo ""
    echo -e "${YELLOW}⚠️  IMPORTANTE:${NC}"
    echo "   • Ahora SÍ verás tu sistema al bootear"
    echo "   • Espera ~10 segundos para discovery"
    echo "   • Para red entre nodos, usa VirtualBox con Bridge"
    echo ""
else
    echo -e "${RED}Error: No se pudo crear ISO${NC}"
    exit 1
fi