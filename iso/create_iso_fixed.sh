#!/bin/bash

# ========================================
# Script CORREGIDO para crear ISO funcional
# Soluciona problemas de pantalla negra
# ========================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     Creador de ISO CORREGIDO - SO Descentralizado         ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# ========================================
# 1. COMPILAR EL BINARIO ESTÁTICO
# ========================================

echo -e "${YELLOW}[1/8]${NC} Compilando sistema operativo (versión mejorada)..."

# Compilar con más compatibilidad y estático
gcc -Wall -O2 -static -pthread \
    -o decentralized_os_static \
    src/main.c \
    -lm -lrt \
    || { echo -e "${RED}Error compilando${NC}"; exit 1; }

echo -e "${GREEN}  ✓${NC} Binario estático creado"

# ========================================
# 2. CREAR INITRAMFS MEJORADO
# ========================================

echo -e "${YELLOW}[2/8]${NC} Creando initramfs mejorado..."

rm -rf initramfs_new
mkdir -p initramfs_new/{bin,sbin,etc,proc,sys,dev,tmp,var,lib,lib64,root}

# Copiar binario principal
cp decentralized_os_static initramfs_new/sbin/dos_main
chmod +x initramfs_new/sbin/dos_main

# Descargar busybox si no existe
if [ ! -f "busybox" ]; then
    echo -e "${YELLOW}  Descargando busybox...${NC}"
    wget -q https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox \
        || { echo "Error descargando busybox"; exit 1; }
fi
chmod +x busybox
cp busybox initramfs_new/bin/

# Crear enlaces simbólicos importantes
cd initramfs_new/bin
for cmd in sh ash bash ls cat echo mkdir rm cp mv mount umount ifconfig ip \
           ping wget ps top kill sleep grep sed awk; do
    ln -sf busybox $cmd 2>/dev/null || true
done
cd ../..

# Crear script de inicio MEJORADO
cat > initramfs_new/init << 'EOFSCRIPT'
#!/bin/busybox sh

# ========================================
# Script de Inicio del SO Descentralizado
# ========================================

echo "=========================================="
echo "  Sistema Operativo Descentralizado v1.0"
echo "=========================================="
echo ""

# Configurar PATH
export PATH=/bin:/sbin:/usr/bin:/usr/sbin

# Montar sistemas de archivos esenciales
echo "[INIT] Montando sistemas de archivos..."
/bin/busybox mount -t proc none /proc 2>/dev/null || true
/bin/busybox mount -t sysfs none /sys 2>/dev/null || true
/bin/busybox mount -t devtmpfs none /dev 2>/dev/null || true
/bin/busybox mount -t tmpfs none /tmp 2>/dev/null || true

echo "[INIT] Sistemas de archivos montados"

# Configurar dispositivos básicos
echo "[INIT] Configurando dispositivos..."
[ -e /dev/null ] || /bin/busybox mknod /dev/null c 1 3
[ -e /dev/zero ] || /bin/busybox mknod /dev/zero c 1 5
[ -e /dev/random ] || /bin/busybox mknod /dev/random c 1 8
chmod 666 /dev/null /dev/zero /dev/random 2>/dev/null || true

# Configurar loopback
echo "[INIT] Configurando red loopback..."
/bin/busybox ifconfig lo 127.0.0.1 up 2>/dev/null || true

# Detectar y configurar interfaces de red
echo "[INIT] Detectando interfaces de red..."
for iface in eth0 enp0s3 ens33; do
    if [ -d "/sys/class/net/$iface" ]; then
        echo "[INIT] Configurando $iface..."
        /bin/busybox ifconfig $iface up 2>/dev/null || true
        /bin/busybox udhcpc -i $iface -n -q -t 5 2>/dev/null &
    fi
done

# Esperar un poco para la red
sleep 2

echo ""
echo "[INIT] Sistema inicializado. Iniciando SO Descentralizado..."
echo ""

# Ejecutar el sistema operativo principal
cd /root
exec /sbin/dos_main

# Si falla, dar shell
echo ""
echo "============================================"
echo "  MODO DE EMERGENCIA"
echo "============================================"
echo ""
echo "El sistema principal falló. Shell disponible."
echo "Comandos disponibles: ls, cat, ps, ifconfig"
echo ""

exec /bin/busybox sh
EOFSCRIPT

chmod +x initramfs_new/init

# Crear archivos de configuración
cat > initramfs_new/etc/passwd << 'EOF'
root:x:0:0:root:/root:/bin/sh
EOF

cat > initramfs_new/etc/group << 'EOF'
root:x:0:
EOF

# Crear initramfs
echo -e "${YELLOW}  Empaquetando initramfs...${NC}"
cd initramfs_new
find . | cpio -o -H newc 2>/dev/null | gzip -9 > ../initramfs_fixed.cpio.gz
cd ..

echo -e "${GREEN}  ✓${NC} initramfs mejorado creado"

# ========================================
# 3. OBTENER KERNEL COMPATIBLE
# ========================================

echo -e "${YELLOW}[3/8]${NC} Preparando kernel..."

KERNEL_FOUND=0

# Intentar usar kernel del sistema
if [ -f "/boot/vmlinuz-$(uname -r)" ]; then
    echo -e "${GREEN}  ✓${NC} Usando kernel del sistema: $(uname -r)"
    cp "/boot/vmlinuz-$(uname -r)" vmlinuz_fixed
    KERNEL_FOUND=1
elif [ -f "/boot/vmlinuz" ]; then
    echo -e "${GREEN}  ✓${NC} Usando /boot/vmlinuz"
    cp "/boot/vmlinuz" vmlinuz_fixed
    KERNEL_FOUND=1
fi

# Si no hay kernel, intentar descargar uno
if [ $KERNEL_FOUND -eq 0 ]; then
    echo -e "${YELLOW}  ! Descargando kernel compatible...${NC}"
    
    # Intentar descargar kernel de Alpine Linux (pequeño y compatible)
    KERNEL_URL="https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/x86_64/alpine-virt-3.18.0-x86_64.iso"
    
    echo "    Esto puede tardar unos minutos..."
    wget -q -O alpine.iso "$KERNEL_URL" || {
        echo -e "${RED}  ✗ No se pudo descargar kernel${NC}"
        echo ""
        echo "SOLUCIÓN MANUAL:"
        echo "  Copia un kernel desde /boot/ de tu sistema:"
        echo "  sudo cp /boot/vmlinuz-\$(uname -r) vmlinuz_fixed"
        exit 1
    }
    
    # Extraer kernel de Alpine
    mkdir -p alpine_mount
    sudo mount -o loop alpine.iso alpine_mount 2>/dev/null
    cp alpine_mount/boot/vmlinuz-virt vmlinuz_fixed
    sudo umount alpine_mount
    rm -rf alpine_mount alpine.iso
    
    KERNEL_FOUND=1
    echo -e "${GREEN}  ✓${NC} Kernel descargado"
fi

# ========================================
# 4. CREAR ESTRUCTURA ISO
# ========================================

echo -e "${YELLOW}[4/8]${NC} Creando estructura ISO..."

rm -rf iso_fixed
mkdir -p iso_fixed/boot/grub

# Copiar archivos
cp vmlinuz_fixed iso_fixed/boot/vmlinuz
cp initramfs_fixed.cpio.gz iso_fixed/boot/initramfs.gz

# Configuración GRUB mejorada
cat > iso_fixed/boot/grub/grub.cfg << 'EOFGRUB'
set timeout=5
set default=0

menuentry "Sistema Operativo Descentralizado" {
    echo "Cargando kernel..."
    linux /boot/vmlinuz console=ttyS0,115200 console=tty0 debug
    echo "Cargando initramfs..."
    initrd /boot/initramfs.gz
    echo "Iniciando sistema..."
}

menuentry "SO Descentralizado (Modo Verbose)" {
    linux /boot/vmlinuz console=ttyS0,115200 console=tty0 debug loglevel=7
    initrd /boot/initramfs.gz
}

menuentry "SO Descentralizado (Modo Seguro)" {
    linux /boot/vmlinuz console=ttyS0,115200 console=tty0 single debug
    initrd /boot/initramfs.gz
}
EOFGRUB

echo -e "${GREEN}  ✓${NC} Estructura ISO creada"

# ========================================
# 5. GENERAR ISO
# ========================================

echo -e "${YELLOW}[5/8]${NC} Generando imagen ISO..."

ISO_NAME="decentralized_os_fixed.iso"

# Usar grub-mkrescue
if command -v grub-mkrescue &> /dev/null; then
    grub-mkrescue -o $ISO_NAME iso_fixed/ 2>/dev/null || {
        echo -e "${YELLOW}  ! Intentando método alternativo...${NC}"
        xorriso -as mkisofs \
            -r -V "DOS_FIXED" \
            -b boot/grub/i386-pc/eltorito.img \
            -no-emul-boot \
            -boot-load-size 4 \
            -boot-info-table \
            -o $ISO_NAME \
            iso_fixed/
    }
else
    xorriso -as mkisofs \
        -r -V "DOS_FIXED" \
        -o $ISO_NAME \
        iso_fixed/
fi

SIZE=$(du -h $ISO_NAME | cut -f1)
echo -e "${GREEN}  ✓${NC} ISO creada: $ISO_NAME ($SIZE)"

# ========================================
# 6. CREAR SCRIPT DE PRUEBA QEMU
# ========================================

echo -e "${YELLOW}[6/8]${NC} Creando script de prueba..."

cat > test_iso.sh << 'EOFTEST'
#!/bin/bash

ISO="decentralized_os_fixed.iso"

echo "Probando ISO en QEMU..."
echo "Configuración:"
echo "  - 1GB RAM"
echo "  - 1 CPU"
echo "  - Red: user mode"
echo "  - Serial console habilitado"
echo ""
echo "Presiona Ctrl+A, X para salir de QEMU"
echo ""

qemu-system-x86_64 \
    -cdrom $ISO \
    -m 1024 \
    -smp 1 \
    -enable-kvm \
    -netdev user,id=net0 \
    -device e1000,netdev=net0 \
    -serial stdio \
    -vga std
EOFTEST

chmod +x test_iso.sh

echo -e "${GREEN}  ✓${NC} Script de prueba creado: test_iso.sh"

# ========================================
# 7. CREAR GUÍA DE CONFIGURACIÓN VIRTUALBOX
# ========================================

echo -e "${YELLOW}[7/8]${NC} Creando guía de VirtualBox..."

cat > VIRTUALBOX_SETUP.md << 'EOFVB'
# Configuración de VirtualBox para SO Descentralizado

## Pasos para crear la VM:

### 1. Crear Nueva Máquina Virtual
```
Nombre: SO_Descentralizado
Tipo: Linux
Versión: Other Linux (64-bit)
```

### 2. Configuración de Memoria
```
RAM: Mínimo 512 MB, Recomendado 1024 MB
```

### 3. Disco Duro
```
- No crear disco duro (bootearemos desde ISO)
- O crear un disco de 1GB si quieres persistencia
```

### 4. Configuración de Sistema
```
Sistema > Placa Base:
  - Memoria Base: 1024 MB
  - Orden de arranque: CD/DVD primero
  - Chipset: PIIX3
  - Extended Features: Habilitar I/O APIC

Sistema > Procesador:
  - CPUs: 1
  - Habilitar PAE/NX

Sistema > Aceleración:
  - Interfaz de paravirtualización: Default
  - Habilitar VT-x/AMD-V si está disponible
```

### 5. Configuración de Pantalla
```
Pantalla > Screen:
  - Video Memory: 16 MB
  - Graphics Controller: VMSVGA o VBoxVGA
  - Habilitar 3D Acceleration: NO
```

### 6. Configuración de Red
```
Red > Adaptador 1:
  - Conectado a: NAT o Bridge
  - Tipo de adaptador: Intel PRO/1000 MT Desktop
```

### 7. Almacenamiento
```
Storage > Controller: IDE:
  - Agregar unidad óptica
  - Seleccionar: decentralized_os_fixed.iso
```

### 8. Serial Port (Opcional, para debugging)
```
Serial Ports > Port 1:
  - Habilitar Serial Port
  - Port Mode: Raw File
  - Path: /tmp/dos_serial.log
```

## Iniciar la VM

1. Selecciona la VM "SO_Descentralizado"
2. Click en "Iniciar"
3. En el menú GRUB, selecciona la primera opción
4. Espera a que cargue (puede tardar 10-30 segundos)

## Solución de Problemas

### Pantalla Negra:
- Verifica que la ISO está montada en el controlador IDE
- Prueba con "Modo Verbose" del menú GRUB
- Revisa el archivo de log serial si lo configuraste

### No aparece GRUB:
- Verifica el orden de arranque (CD/DVD primero)
- Rehaz la ISO con: ./create_iso_fixed.sh

### "VT-x is disabled":
- Habilita la virtualización en la BIOS de tu PC
- O deshabilita "Enable VT-x/AMD-V" en VirtualBox

### Kernel Panic:
- Usa un kernel diferente (copia uno de /boot/)
- Prueba con el modo "Seguro"

## Comandos Útiles en el Sistema

Una vez que arranque, tendrás acceso a:
```
status  - Ver estado de la red
nodes   - Ver nodos activos  
task <descripción> - Crear tarea
tasks   - Ver tareas
help    - Ayuda
exit    - Salir
```

## Probar con Múltiples VMs

Para crear una red:
1. Crea 2-3 VMs con esta ISO
2. Configura todas en modo "Bridge" o "Red Interna"
3. Iníc ialas simultáneamente
4. Se descubrirán automáticamente
EOFVB

echo -e "${GREEN}  ✓${NC} Guía creada: VIRTUALBOX_SETUP.md"

# ========================================
# 8. RESUMEN
# ========================================

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}          ✅ ISO CORREGIDA CREADA                          ${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${BLUE}Archivos generados:${NC}"
echo "  📀 $ISO_NAME - ISO corregida"
echo "  📄 VIRTUALBOX_SETUP.md - Guía de configuración"
echo "  🧪 test_iso.sh - Script de prueba en QEMU"
echo ""
echo -e "${BLUE}Próximos pasos:${NC}"
echo ""
echo "1. PROBAR EN QEMU (recomendado primero):"
echo -e "   ${YELLOW}./test_iso.sh${NC}"
echo ""
echo "2. PROBAR EN VIRTUALBOX:"
echo -e "   ${YELLOW}less VIRTUALBOX_SETUP.md${NC}"
echo "   - Sigue las instrucciones paso a paso"
echo ""
echo "3. SI AÚN NO FUNCIONA:"
echo "   - Revisa los logs con el serial port"
echo "   - Prueba el modo Verbose del GRUB"
echo "   - Verifica: cat /var/log/vbox.log"
echo ""
echo -e "${GREEN}¡Ahora debería funcionar correctamente!${NC}"
