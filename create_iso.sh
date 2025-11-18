#!/bin/bash

# ========================================
# Script para crear ISO booteable del SO Descentralizado
# ========================================

set -e

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║         Creador de ISO - SO Descentralizado               ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# Verificar permisos de root (necesario para algunas operaciones)
if [ "$EUID" -ne 0 ]; then 
    echo -e "${YELLOW}[!] Recomendado ejecutar como root para todas las funciones${NC}"
fi

# Verificar dependencias
echo -e "${YELLOW}[1/10]${NC} Verificando dependencias..."

check_command() {
    if command -v $1 &> /dev/null; then
        echo -e "${GREEN}  ✓${NC} $1"
        return 0
    else
        echo -e "${RED}  ✗${NC} $1 no encontrado"
        return 1
    fi
}

MISSING=0
check_command gcc || MISSING=1
check_command make || MISSING=1
check_command xorriso || MISSING=1
check_command grub-mkrescue || MISSING=1
check_command mtools || MISSING=1
check_command qemu-system-x86_64 || echo -e "${YELLOW}  !${NC} QEMU no instalado (opcional)"

if [ $MISSING -eq 1 ]; then
    echo ""
    echo -e "${YELLOW}Instalar dependencias faltantes:${NC}"
    echo "  Ubuntu/Debian:"
    echo "    sudo apt-get install build-essential xorriso grub-pc-bin grub-efi-amd64-bin mtools qemu-system-x86"
    echo "  Fedora:"
    echo "    sudo dnf install gcc make xorriso grub2-tools-extra mtools qemu-system-x86"
    echo "  Arch:"
    echo "    sudo pacman -S gcc make libisoburn grub mtools qemu"
    echo ""
    read -p "¿Continuar de todos modos? (s/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Ss]$ ]]; then
        exit 1
    fi
fi

# ========================================
# COMPILAR EL SISTEMA
# ========================================

echo -e "${YELLOW}[2/10]${NC} Compilando el sistema operativo..."

# Compilar versión con red real
if [ -f "src/main_network.c" ]; then
    gcc -O2 -static -pthread -o decentralized_os src/main_network.c -lm -lrt
elif [ -f "main_network.c" ]; then
    gcc -O2 -static -pthread -o decentralized_os main_network.c -lm -lrt
else
    echo -e "${RED}Error: No se encuentra main_network.c${NC}"
    exit 1
fi

echo -e "${GREEN}  ✓${NC} Sistema compilado"

# ========================================
# CREAR INITRAMFS
# ========================================

echo -e "${YELLOW}[3/10]${NC} Creando initramfs..."

# Crear estructura de directorios
mkdir -p initramfs/{bin,sbin,etc,proc,sys,dev,tmp,var,usr,lib,lib64,root,home,mnt}

# Copiar binario principal
cp decentralized_os initramfs/sbin/init
chmod +x initramfs/sbin/init

# Crear script de inicio
cat > initramfs/init << 'EOF'
#!/bin/sh

# Script de inicio del SO Descentralizado

echo "Iniciando Sistema Operativo Descentralizado..."

# Montar sistemas de archivos esenciales
/bin/busybox mount -t proc none /proc
/bin/busybox mount -t sysfs none /sys
/bin/busybox mount -t devtmpfs none /dev

# Configurar red
/bin/busybox ifconfig lo 127.0.0.1 up
/bin/busybox ifconfig eth0 up
/bin/busybox udhcpc -i eth0 -s /usr/share/udhcpc/default.script

# Iniciar el sistema descentralizado
echo "Iniciando núcleo descentralizado..."
/sbin/init

# Si el sistema falla, dar shell de emergencia
echo "Sistema detenido. Shell de emergencia:"
/bin/busybox sh
EOF
chmod +x initramfs/init

# Descargar busybox estático si no existe
if [ ! -f "busybox" ]; then
    echo -e "${YELLOW}  Descargando busybox...${NC}"
    wget -q https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox
    chmod +x busybox
fi
cp busybox initramfs/bin/

# Crear enlaces simbólicos para comandos busybox
for cmd in sh cat ls mkdir rm cp mv mount umount ifconfig ping wget; do
    ln -sf busybox initramfs/bin/$cmd
done

# Crear archivos de configuración básicos
cat > initramfs/etc/passwd << 'EOF'
root:x:0:0:root:/root:/bin/sh
daemon:x:1:1:daemon:/usr/sbin:/bin/false
EOF

cat > initramfs/etc/group << 'EOF'
root:x:0:
daemon:x:1:
EOF

cat > initramfs/etc/fstab << 'EOF'
proc  /proc  proc  defaults  0 0
sysfs /sys   sysfs defaults  0 0
none  /dev   devtmpfs defaults 0 0
EOF

# Crear archivo de configuración de red
cat > initramfs/etc/network.conf << 'EOF'
# Configuración de red para SO Descentralizado
DISCOVERY_PORT=8888
DATA_PORT=8889
BROADCAST_ENABLED=1
AUTO_DISCOVERY=1
EOF

# Crear initramfs.cpio.gz
cd initramfs
find . | cpio -o -H newc | gzip > ../initramfs.cpio.gz
cd ..

echo -e "${GREEN}  ✓${NC} initramfs creado"

# ========================================
# CREAR KERNEL MÍNIMO (Opcional)
# ========================================

echo -e "${YELLOW}[4/10]${NC} Preparando kernel..."

# Si existe un kernel Linux, usarlo. Si no, descargar uno mínimo
if [ -f "/boot/vmlinuz-$(uname -r)" ]; then
    echo -e "${GREEN}  ✓${NC} Usando kernel del sistema: $(uname -r)"
    cp "/boot/vmlinuz-$(uname -r)" vmlinuz
elif [ -f "vmlinuz" ]; then
    echo -e "${GREEN}  ✓${NC} Usando kernel existente"
else
    echo -e "${YELLOW}  !${NC} No se encontró kernel, descargando kernel mínimo..."
    # Aquí podrías descargar un kernel precompilado mínimo
    echo -e "${RED}  ✗${NC} Necesitas proporcionar un kernel Linux (vmlinuz)"
    echo "    Copia uno desde /boot/ o compila uno mínimo"
    exit 1
fi

# ========================================
# CREAR ESTRUCTURA ISO
# ========================================

echo -e "${YELLOW}[5/10]${NC} Creando estructura ISO..."

# Limpiar y crear directorios
rm -rf iso_root
mkdir -p iso_root/{boot/grub,live,isolinux}

# Copiar kernel e initramfs
cp vmlinuz iso_root/boot/
cp initramfs.cpio.gz iso_root/boot/

# Crear configuración de GRUB
cat > iso_root/boot/grub/grub.cfg << 'EOF'
set default=0
set timeout=5

menuentry "Sistema Operativo Descentralizado" {
    linux /boot/vmlinuz quiet
    initrd /boot/initramfs.cpio.gz
}

menuentry "SO Descentralizado (Modo Debug)" {
    linux /boot/vmlinuz debug
    initrd /boot/initramfs.cpio.gz
}

menuentry "SO Descentralizado (Modo Seguro)" {
    linux /boot/vmlinuz single
    initrd /boot/initramfs.cpio.gz
}
EOF

echo -e "${GREEN}  ✓${NC} Estructura ISO creada"

# ========================================
# CREAR CONFIGURACIÓN ISOLINUX (Legacy BIOS)
# ========================================

echo -e "${YELLOW}[6/10]${NC} Configurando arranque Legacy BIOS..."

# Copiar archivos de isolinux si están disponibles
if [ -f "/usr/lib/ISOLINUX/isolinux.bin" ]; then
    cp /usr/lib/ISOLINUX/isolinux.bin iso_root/isolinux/
    cp /usr/lib/syslinux/modules/bios/* iso_root/isolinux/ 2>/dev/null || true
fi

# Crear configuración isolinux
cat > iso_root/isolinux/isolinux.cfg << 'EOF'
DEFAULT dos
PROMPT 1
TIMEOUT 50

LABEL dos
    MENU LABEL Sistema Operativo Descentralizado
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initramfs.cpio.gz quiet

LABEL debug
    MENU LABEL SO Descentralizado (Debug)
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initramfs.cpio.gz debug
EOF

echo -e "${GREEN}  ✓${NC} Configuración Legacy BIOS lista"

# ========================================
# AÑADIR APLICACIONES Y UTILIDADES
# ========================================

echo -e "${YELLOW}[7/10]${NC} Añadiendo utilidades..."

mkdir -p iso_root/apps

# Crear script de configuración de red
cat > iso_root/apps/setup_network.sh << 'EOF'
#!/bin/sh
echo "Configurando red para SO Descentralizado..."
ifconfig eth0 up
udhcpc -i eth0
echo "Red configurada. IP:"
ifconfig eth0 | grep "inet addr"
EOF
chmod +x iso_root/apps/setup_network.sh

# Crear README
cat > iso_root/README.txt << 'EOF'
========================================
Sistema Operativo Descentralizado v1.0
========================================

INSTRUCCIONES DE USO:

1. Arrancar desde esta ISO
2. El sistema detectará automáticamente otros nodos en la red
3. Usa los comandos:
   - status: Ver estado de la red
   - nodes: Ver nodos activos
   - task <desc>: Crear tarea distribuida
   - help: Ver ayuda

REQUISITOS:
- Conexión de red Ethernet o WiFi
- Mínimo 512MB RAM
- Procesador x86_64

CONFIGURACIÓN DE RED:
El sistema usa los puertos:
- 8888: Discovery (UDP Broadcast)
- 8889: Datos (TCP)

Asegúrate de que estos puertos no estén bloqueados.

========================================
EOF

echo -e "${GREEN}  ✓${NC} Utilidades añadidas"

# ========================================
# GENERAR ISO
# ========================================

echo -e "${YELLOW}[8/10]${NC} Generando imagen ISO..."

ISO_NAME="decentralized_os.iso"

# Intentar con grub-mkrescue primero (más compatible)
if command -v grub-mkrescue &> /dev/null; then
    grub-mkrescue -o $ISO_NAME iso_root/ 2>/dev/null || {
        echo -e "${YELLOW}  !${NC} grub-mkrescue falló, intentando con xorriso..."
        xorriso -as mkisofs \
            -iso-level 3 \
            -full-iso9660-filenames \
            -volid "DECENTRALIZED_OS" \
            -eltorito-boot boot/grub/eltorito.img \
            -no-emul-boot \
            -boot-load-size 4 \
            -boot-info-table \
            -eltorito-catalog boot/grub/boot.cat \
            -output $ISO_NAME \
            iso_root
    }
else
    # Usar xorriso directamente
    xorriso -as mkisofs \
        -iso-level 3 \
        -full-iso9660-filenames \
        -volid "DECENTRALIZED_OS" \
        -output $ISO_NAME \
        iso_root
fi

if [ -f "$ISO_NAME" ]; then
    SIZE=$(du -h $ISO_NAME | cut -f1)
    echo -e "${GREEN}  ✓${NC} ISO creada: $ISO_NAME ($SIZE)"
else
    echo -e "${RED}  ✗${NC} Error creando ISO"
    exit 1
fi

# ========================================
# CREAR SCRIPT DE ARRANQUE USB
# ========================================

echo -e "${YELLOW}[9/10]${NC} Creando script para USB booteable..."

cat > create_usb.sh << 'EOF'
#!/bin/bash
# Script para crear USB booteable

if [ "$#" -ne 1 ]; then
    echo "Uso: $0 /dev/sdX"
    echo "Donde /dev/sdX es tu dispositivo USB (ej: /dev/sdb)"
    exit 1
fi

DEVICE=$1
ISO="decentralized_os.iso"

echo "⚠️  ADVERTENCIA: Esto borrará todos los datos en $DEVICE"
read -p "¿Continuar? (s/n): " -n 1 -r
echo

if [[ $REPLY =~ ^[Ss]$ ]]; then
    echo "Escribiendo ISO en $DEVICE..."
    sudo dd if=$ISO of=$DEVICE bs=4M status=progress conv=fdatasync
    echo "✅ USB booteable creado"
else
    echo "Cancelado"
fi
EOF
chmod +x create_usb.sh

echo -e "${GREEN}  ✓${NC} Script USB creado"

# ========================================
# PRUEBA EN QEMU (Opcional)
# ========================================

echo -e "${YELLOW}[10/10]${NC} Configuración completa"

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}          ✅ ISO CREADA EXITOSAMENTE                       ${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${BLUE}Archivos generados:${NC}"
echo "  📀 $ISO_NAME - Imagen ISO booteable"
echo "  🔧 create_usb.sh - Script para crear USB booteable"
echo "  📁 iso_root/ - Contenido de la ISO"
echo "  📦 initramfs.cpio.gz - Sistema de archivos inicial"
echo ""
echo -e "${BLUE}Para usar:${NC}"
echo ""
echo "1. Probar en máquina virtual (QEMU):"
echo -e "   ${YELLOW}qemu-system-x86_64 -cdrom $ISO_NAME -m 1024 -netdev user,id=net0 -device e1000,netdev=net0${NC}"
echo ""
echo "2. Probar con múltiples VMs en red:"
echo -e "   ${YELLOW}./test_network_vms.sh${NC}"
echo ""
echo "3. Grabar en CD/DVD:"
echo -e "   ${YELLOW}cdrecord -v dev=/dev/cdrom $ISO_NAME${NC}"
echo ""
echo "4. Crear USB booteable:"
echo -e "   ${YELLOW}sudo ./create_usb.sh /dev/sdX${NC}"
echo "   (Reemplaza /dev/sdX con tu dispositivo USB)"
echo ""
echo "5. Usar en VirtualBox/VMware:"
echo "   - Crear nueva VM Linux 64-bit"
echo "   - Asignar mínimo 512MB RAM"
echo "   - Montar la ISO como CD/DVD"
echo "   - Configurar red en modo Bridge"
echo ""

# Crear script para probar red con múltiples VMs
cat > test_network_vms.sh << 'EOF'
#!/bin/bash
# Probar con 3 VMs en red local

echo "Iniciando red virtual..."

# Crear bridge de red
sudo ip link add name br0 type bridge
sudo ip addr add 192.168.100.1/24 dev br0
sudo ip link set br0 up

# Iniciar 3 VMs
for i in 1 2 3; do
    echo "Iniciando VM $i..."
    qemu-system-x86_64 \
        -cdrom decentralized_os.iso \
        -m 512 \
        -netdev tap,id=net$i,ifname=tap$i,script=no,downscript=no \
        -device e1000,netdev=net$i,mac=52:54:00:12:34:0$i \
        -daemonize \
        -vnc :$i &
    
    # Configurar interfaz tap
    sudo ip tuntap add tap$i mode tap
    sudo ip link set tap$i master br0
    sudo ip link set tap$i up
done

echo ""
echo "✅ 3 VMs iniciadas"
echo "Conéctate con VNC a:"
echo "  VM1: localhost:5901"
echo "  VM2: localhost:5902"  
echo "  VM3: localhost:5903"
echo ""
echo "Para detener: sudo killall qemu-system-x86_64"
EOF
chmod +x test_network_vms.sh

echo -e "${GREEN}¡Sistema listo para usar en red real!${NC}"