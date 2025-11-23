#!/bin/bash
# Script ULTRA SIMPLE - Sistema mínimo desde cero
# NO usa Alpine, solo tu código + busybox + kernel

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

clear
echo -e "${GREEN}"
cat << "EOF"
╔═══════════════════════════════════════════════════════════╗
║   CONSTRUCTOR MINIMALISTA - SOLO TU CÓDIGO                ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# ========================================
# PASO 1: COMPILAR
# ========================================

echo -e "${YELLOW}[1/5]${NC} Compilando tu sistema..."

if [ ! -f "src/main_network.c" ]; then
    echo -e "${RED}Error: src/main_network.c no existe${NC}"
    exit 1
fi

gcc -static -O2 -pthread -o dos_system src/main_network.c -lm || {
    echo -e "${RED}Error compilando${NC}"
    exit 1
}

echo -e "${GREEN}  ✓ Compilado: dos_system${NC}"

# ========================================
# PASO 2: DESCARGAR BUSYBOX
# ========================================

echo -e "${YELLOW}[2/5]${NC} Descargando busybox..."

if [ ! -f "busybox" ]; then
    wget -q https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox
    chmod +x busybox
fi

echo -e "${GREEN}  ✓ Busybox listo${NC}"

# ========================================
# PASO 3: CREAR INITRAMFS
# ========================================

echo -e "${YELLOW}[3/5]${NC} Creando initramfs..."

rm -rf initramfs
mkdir -p initramfs/{bin,sbin,etc,proc,sys,dev,root,tmp}

# Copiar busybox
cp busybox initramfs/bin/

# Crear enlaces para comandos
cd initramfs/bin
for cmd in sh ash cat ls mkdir rm cp mv mount umount \
           ifconfig ip route ping wget udhcpc sleep echo ps kill; do
    ln -sf busybox $cmd
done
cd ../..

# Copiar nuestro sistema
cp dos_system initramfs/sbin/

# Crear script init (PUNTO DE ENTRADA)
cat > initramfs/init << 'EOFINIT'
#!/bin/sh
# Init principal - Ejecuta el SO Descentralizado

# Montar filesystems
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

# Banner
clear
cat << 'BANNER'
╔═══════════════════════════════════════════════════════════╗
║        SISTEMA OPERATIVO DESCENTRALIZADO v1.0             ║
║              Red Ad-Hoc Automática                        ║
╚═══════════════════════════════════════════════════════════╝
BANNER

echo ""
echo "═══════════════════════════════════════════════════════"
echo "           Configurando Red Ad-Hoc..."
echo "═══════════════════════════════════════════════════════"
echo ""

# Configurar loopback
ifconfig lo 127.0.0.1 up

# Configurar todas las interfaces
for iface in eth0 eth1 wlan0 enp0s3; do
    if [ -e "/sys/class/net/$iface" ]; then
        echo "→ Configurando $iface..."
        ifconfig $iface up 2>/dev/null
        
        # Intentar DHCP
        udhcpc -i $iface -n -q 2>/dev/null &
    fi
done

echo ""
echo "Esperando configuración de red..."
sleep 4

# Mostrar configuración
echo ""
echo "═══════════════════════════════════════════════════════"
echo "           Configuración de Red"
echo "═══════════════════════════════════════════════════════"
ifconfig | grep -A1 "eth0\|wlan0\|enp0s3" | grep "inet addr" | awk '{print "  IP: " $2}' | sed 's/addr://'

echo ""
echo "═══════════════════════════════════════════════════════"
echo "           Puertos Utilizados"
echo "═══════════════════════════════════════════════════════"
echo "  • 8888/UDP - Descubrimiento de nodos"
echo "  • 8889/TCP - Transferencia de datos"
echo ""
echo "═══════════════════════════════════════════════════════"
echo ""

# Pequeña pausa para que se vea
sleep 2

# EJECUTAR SISTEMA DESCENTRALIZADO
echo "Iniciando Sistema Operativo Descentralizado..."
echo ""

/sbin/dos_system

# Si el sistema termina, dar shell
echo ""
echo "Sistema detenido. Shell de emergencia:"
/bin/sh
EOFINIT

chmod +x initramfs/init

# Crear initramfs.cpio.gz
cd initramfs
find . -print0 | cpio --null -o --format=newc 2>/dev/null | gzip -9 > ../initramfs.cpio.gz
cd ..

echo -e "${GREEN}  ✓ Initramfs creado${NC}"

# ========================================
# PASO 4: OBTENER KERNEL
# ========================================

echo -e "${YELLOW}[4/5]${NC} Verificando kernel..."

# Intentar copiar kernel del sistema
KERNEL_FOUND=0

if [ -f "/boot/vmlinuz-$(uname -r)" ]; then
    cp "/boot/vmlinuz-$(uname -r)" vmlinuz
    KERNEL_FOUND=1
elif [ -f "/boot/vmlinuz" ]; then
    cp /boot/vmlinuz vmlinuz
    KERNEL_FOUND=1
else
    # Buscar cualquier kernel en /boot
    KERNEL=$(ls /boot/vmlinuz-* 2>/dev/null | head -1)
    if [ -n "$KERNEL" ]; then
        cp "$KERNEL" vmlinuz
        KERNEL_FOUND=1
    fi
fi

if [ $KERNEL_FOUND -eq 0 ]; then
    echo -e "${RED}No se encontró kernel en /boot/${NC}"
    echo "Opciones:"
    echo "  1. Copia un vmlinuz manualmente a este directorio"
    echo "  2. Instala linux-image en tu sistema"
    exit 1
fi

echo -e "${GREEN}  ✓ Kernel listo${NC}"

# ========================================
# PASO 5: CREAR ISO
# ========================================

echo -e "${YELLOW}[5/5]${NC} Generando ISO..."

# Crear estructura ISO
rm -rf iso_root
mkdir -p iso_root/boot/grub

cp vmlinuz iso_root/boot/
cp initramfs.cpio.gz iso_root/boot/

# Configuración GRUB
cat > iso_root/boot/grub/grub.cfg << 'EOFGRUB'
set default=0
set timeout=2

menuentry "Sistema Operativo Descentralizado" {
    linux /boot/vmlinuz quiet
    initrd /boot/initramfs.cpio.gz
}

menuentry "SO Descentralizado (Debug)" {
    linux /boot/vmlinuz debug loglevel=7
    initrd /boot/initramfs.cpio.gz
}
EOFGRUB

# README
cat > iso_root/README.txt << 'EOFREADME'
╔═══════════════════════════════════════════════════════════╗
║   SISTEMA OPERATIVO DESCENTRALIZADO                       ║
╚═══════════════════════════════════════════════════════════╝

ESTA ISO EJECUTA TU CÓDIGO DIRECTAMENTE

Al bootear verás:
1. Banner del sistema
2. Configuración de red automática
3. Tu interfaz de comandos

COMANDOS:
  status - Estado del nodo
  nodes  - Nodos descubiertos
  task   - Crear tarea
  help   - Ayuda
  quit   - Salir

PRUEBA:
- Inicia 2-3 VMs con esta ISO
- Configura red en modo Bridge
- Espera ~10 segundos
- Los nodos se verán automáticamente
EOFREADME

# Generar ISO
OUTPUT_ISO="decentralized_os_minimal.iso"

if command -v grub-mkrescue &> /dev/null; then
    grub-mkrescue -o "$OUTPUT_ISO" iso_root/ 2>&1 | grep -v "warning" || {
        echo -e "${YELLOW}  Reintentando con xorriso...${NC}"
        xorriso -as mkisofs -o "$OUTPUT_ISO" iso_root/
    }
else
    xorriso -as mkisofs -o "$OUTPUT_ISO" iso_root/
fi

# Verificar
if [ -f "$OUTPUT_ISO" ]; then
    SIZE=$(du -h "$OUTPUT_ISO" | cut -f1)
    
    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║            ✅ ISO MINIMALISTA CREADA!                    ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "📀 Archivo: $OUTPUT_ISO ($SIZE)"
    echo ""
    echo -e "${BLUE}Esta ISO ejecuta SOLO tu código, nada más.${NC}"
    echo ""
    echo "🧪 PROBAR:"
    echo "   qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 1024 -enable-kvm"
    echo ""
    echo "🌐 PROBAR RED (VirtualBox RECOMENDADO):"
    echo "   1. Crear VM con esta ISO"
    echo "   2. Configurar Red → Bridge"
    echo "   3. Clonar VM 2-3 veces"
    echo "   4. Iniciar todas"
    echo ""
else
    echo -e "${RED}Error creando ISO${NC}"
    exit 1
fi