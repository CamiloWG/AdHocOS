#!/bin/bash

# ========================================
# Script CORREGIDO para crear ISO del SO Descentralizado
# Soluciona el problema de boot con Alpine Linux
# ========================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

ALPINE_VERSION="3.18"
ALPINE_ISO="alpine-standard-${ALPINE_VERSION}.0-x86_64.iso"
ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_VERSION}/releases/x86_64/${ALPINE_ISO}"
OUTPUT_ISO="dos_virtualbox.iso"

echo -e "${CYAN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║   GENERADOR DE ISO - SISTEMA OPERATIVO DESCENTRALIZADO   ║
║   Versión CORREGIDA para VirtualBox                      ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}\n"

# ========================================
# PASO 1: VERIFICAR DEPENDENCIAS
# ========================================

echo -e "${YELLOW}[1/9]${NC} Verificando dependencias..."

check_command() {
    if command -v $1 &> /dev/null; then
        echo -e "  ${GREEN}✓${NC} $1"
        return 0
    else
        echo -e "  ${RED}✗${NC} $1 no encontrado"
        return 1
    fi
}

MISSING=0
check_command gcc || MISSING=1
check_command wget || MISSING=1
check_command xorriso || MISSING=1
check_command cpio || MISSING=1
check_command gzip || MISSING=1

if [ $MISSING -eq 1 ]; then
    echo -e "${RED}Faltan dependencias. Instala:${NC}"
    echo "  Ubuntu/Debian: sudo apt-get install build-essential wget xorriso cpio gzip"
    echo "  Fedora: sudo dnf install gcc wget xorriso cpio gzip"
    exit 1
fi

echo -e "${GREEN}  ✓ Todas las dependencias presentes${NC}\n"

# ========================================
# PASO 2: DESCARGAR ALPINE LINUX
# ========================================

echo -e "${YELLOW}[2/9]${NC} Descargando Alpine Linux..."

if [ ! -f "$ALPINE_ISO" ]; then
    echo "  Descargando $ALPINE_ISO..."
    wget -q --show-progress "$ALPINE_URL" || {
        echo -e "${RED}Error descargando Alpine${NC}"
        exit 1
    }
    echo -e "${GREEN}  ✓ Descarga completada${NC}"
else
    echo -e "${GREEN}  ✓ Alpine ya descargado${NC}"
fi

echo ""

# ========================================
# PASO 3: COMPILAR EL SISTEMA
# ========================================

echo -e "${YELLOW}[3/9]${NC} Compilando Sistema Operativo..."

if [ ! -f "src/main_alpine.c" ]; then
    echo -e "${RED}Error: No se encuentra src/main_alpine.c${NC}"
    exit 1
fi

echo "  Compilando estáticamente..."
gcc -Wall -Wextra -O2 -static -pthread \
    -o dos_system \
    src/main_alpine.c \
    -lm || {
        echo -e "${RED}Error en compilación${NC}"
        exit 1
    }

# Verificar que el binario es estático
if file dos_system | grep -q "statically linked"; then
    echo -e "${GREEN}  ✓ Binario estático creado correctamente${NC}"
else
    echo -e "${YELLOW}  ⚠ Advertencia: El binario no es completamente estático${NC}"
fi

echo "    Tamaño: $(du -h dos_system | cut -f1)"
echo ""

# ========================================
# PASO 4: EXTRAER ALPINE
# ========================================

echo -e "${YELLOW}[4/9]${NC} Extrayendo Alpine Linux..."

sudo rm -rf alpine_mount alpine_custom iso_work 2>/dev/null || true
mkdir -p alpine_mount alpine_custom

echo "  Montando ISO original..."
sudo mount -o loop "$ALPINE_ISO" alpine_mount || {
    echo -e "${RED}Error montando ISO${NC}"
    exit 1
}

echo "  Copiando archivos..."
sudo rsync -a alpine_mount/ alpine_custom/ || {
    echo -e "${RED}Error copiando archivos${NC}"
    sudo umount alpine_mount
    exit 1
}

sudo umount alpine_mount
rmdir alpine_mount

sudo chmod -R u+w alpine_custom/

echo -e "${GREEN}  ✓ Alpine extraído${NC}\n"

# ========================================
# PASO 5: MODIFICAR INITRAMFS (MÉTODO CORRECTO)
# ========================================

echo -e "${YELLOW}[5/9]${NC} Modificando initramfs..."

# Crear directorio de trabajo
mkdir -p iso_work/initramfs
cd iso_work/initramfs

# Extraer initramfs de Alpine
echo "  Extrayendo initramfs de Alpine..."
if [ -f "../../alpine_custom/boot/initramfs-lts" ]; then
    INITRAMFS_FILE="initramfs-lts"
elif [ -f "../../alpine_custom/boot/initramfs-virt" ]; then
    INITRAMFS_FILE="initramfs-virt"
else
    echo -e "${RED}No se encontró initramfs en Alpine${NC}"
    cd ../..
    exit 1
fi

gzip -dc "../../alpine_custom/boot/$INITRAMFS_FILE" | cpio -idm 2>/dev/null || {
    echo -e "${RED}Error extrayendo initramfs${NC}"
    cd ../..
    exit 1
}

echo "  Añadiendo nuestro sistema..."

# Crear directorio para nuestro sistema
mkdir -p dos/bin dos/config dos/logs

# Copiar binario
cp ../../dos_system dos/bin/
chmod +x dos/bin/dos_system

# Crear script de configuración de red
cat > dos/bin/setup_network.sh << 'EOFNET'
#!/bin/sh
echo "[RED] Configurando red..."

# Configurar loopback
ip link set lo up
ip addr add 127.0.0.1/8 dev lo

# Configurar todas las interfaces ethernet
for iface in $(ls /sys/class/net/ | grep -E '^eth|^enp'); do
    echo "  Activando $iface..."
    ip link set $iface up
    timeout 5 udhcpc -i $iface -n -q 2>/dev/null || \
        ip addr add 192.168.100.$((10 + RANDOM % 200))/24 dev $iface
done

echo "[RED] Red configurada"
EOFNET
chmod +x dos/bin/setup_network.sh

# Crear script de inicio principal
cat > dos/bin/start_dos.sh << 'EOFSTART'
#!/bin/sh
clear

cat << 'BANNER'
╔═══════════════════════════════════════════════════════════╗
║      SISTEMA OPERATIVO DESCENTRALIZADO v1.0              ║
╚═══════════════════════════════════════════════════════════╝
BANNER

echo ""
echo "Iniciando sistema..."

# Configurar red
/dos/bin/setup_network.sh

echo ""
echo "════════════════════════════════════════════════════════"
echo "INFORMACIÓN DEL NODO:"
echo "════════════════════════════════════════════════════════"
echo "Hostname: $(hostname)"
echo "Sistema: Alpine Linux $(cat /etc/alpine-release 2>/dev/null)"
echo "Kernel: $(uname -r)"
echo ""
echo "Red:"
ip addr show | grep -E "inet " | grep -v "127.0.0.1" | awk '{print "  " $2}' || echo "  (configurando...)"
echo ""
echo "⚠️  Puertos: 8888 (UDP Discovery), 8889 (TCP Data)"
echo "════════════════════════════════════════════════════════"
echo ""

sleep 2

echo "Iniciando SO Descentralizado..."
echo ""

cd /dos
exec /dos/bin/dos_system
EOFSTART
chmod +x dos/bin/start_dos.sh

# Modificar init para ejecutar nuestro sistema
echo "  Modificando script de inicio..."

# Buscar y modificar el init de Alpine
if [ -f "init" ]; then
    # Hacer backup
    cp init init.bak
    
    # Añadir nuestra llamada al final del init (antes del exec final)
    # Buscar la línea que ejecuta /sbin/init o similar y añadir antes
    sed -i '/exec \/sbin\/init/i /dos/bin/start_dos.sh' init 2>/dev/null || true
    
    # Si no encuentra esa línea, añadir al final
    echo "" >> init
    echo "# Sistema Operativo Descentralizado" >> init
    echo "/dos/bin/start_dos.sh" >> init
fi

# Reempaquetar initramfs
echo "  Reempaquetando initramfs..."
find . | cpio -o -H newc 2>/dev/null | gzip -9 > ../dos_initramfs.gz

cd ../..

# Reemplazar initramfs en la ISO
sudo cp iso_work/dos_initramfs.gz "alpine_custom/boot/$INITRAMFS_FILE"

echo -e "${GREEN}  ✓ Initramfs modificado${NC}\n"

# ========================================
# PASO 6: MODIFICAR GRUB
# ========================================

echo -e "${YELLOW}[6/9]${NC} Configurando GRUB..."

if [ -f "alpine_custom/boot/grub/grub.cfg" ]; then
    # Hacer backup
    sudo cp alpine_custom/boot/grub/grub.cfg alpine_custom/boot/grub/grub.cfg.bak
    
    # Crear nueva configuración
    sudo tee alpine_custom/boot/grub/grub.cfg > /dev/null << 'EOFGRUB'
set timeout=3
set default=0

menuentry "Sistema Operativo Descentralizado" {
    linux /boot/vmlinuz-lts modules=loop,squashfs,sd-mod,usb-storage quiet
    initrd /boot/initramfs-lts
}

menuentry "SO Descentralizado (Debug)" {
    linux /boot/vmlinuz-lts modules=loop,squashfs,sd-mod,usb-storage console=tty0 debug
    initrd /boot/initramfs-lts
}

menuentry "Alpine Linux (Original)" {
    linux /boot/vmlinuz-lts modules=loop,squashfs,sd-mod,usb-storage quiet
    initrd /boot/initramfs-lts
}
EOFGRUB
    
    echo -e "${GREEN}  ✓ GRUB configurado${NC}"
else
    echo -e "${YELLOW}  ⚠ No se encontró grub.cfg${NC}"
fi

echo ""

# ========================================
# PASO 7: AÑADIR DOCUMENTACIÓN
# ========================================

echo -e "${YELLOW}[7/9]${NC} Añadiendo documentación..."

sudo tee alpine_custom/README.txt > /dev/null << 'EOFREADME'
╔═══════════════════════════════════════════════════════════╗
║   SISTEMA OPERATIVO DESCENTRALIZADO - README              ║
╚═══════════════════════════════════════════════════════════╝

INICIO RÁPIDO:
1. Arrancar desde esta ISO
2. Esperar ~20 segundos
3. El sistema se iniciará automáticamente

COMANDOS:
  status  - Estado del sistema
  nodes   - Nodos activos
  task <desc> - Crear tarea
  tasks   - Ver tareas
  help    - Ayuda
  exit    - Salir

VIRTUALBOX:
- Red: Modo Bridge o Red Interna (NO NAT)
- RAM: Mínimo 512MB, recomendado 1024MB
- CPU: 1-2 cores

PUERTOS:
- 8888/UDP: Discovery
- 8889/TCP: Datos

Si el sistema no inicia automáticamente:
  Login: root (sin password)
  Ejecutar: /dos/bin/start_dos.sh
EOFREADME

echo -e "${GREEN}  ✓ Documentación añadida${NC}\n"

# ========================================
# PASO 8: GENERAR ISO (MÉTODO CORRECTO)
# ========================================

echo -e "${YELLOW}[8/9]${NC} Generando imagen ISO..."

# Método 1: Intentar con la configuración exacta de Alpine
if sudo xorriso -as mkisofs \
    -o "$OUTPUT_ISO" \
    -isohybrid-mbr /usr/lib/syslinux/mbr/isohdpfx.bin 2>/dev/null \
    -c boot/syslinux/boot.cat \
    -b boot/syslinux/isolinux.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    -eltorito-alt-boot \
    -e boot/grub/efi.img \
    -no-emul-boot \
    -isohybrid-gpt-basdat \
    alpine_custom/ 2>&1 | grep -E "Update|Writing" ; then
    
    echo -e "${GREEN}  ✓ ISO creada con soporte EFI/BIOS${NC}"

# Método 2: Si falla, usar configuración simple
elif sudo xorriso -as mkisofs \
    -o "$OUTPUT_ISO" \
    -c boot/syslinux/boot.cat \
    -b boot/syslinux/isolinux.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    alpine_custom/ 2>&1 | grep -E "Update|Writing" ; then
    
    echo -e "${GREEN}  ✓ ISO creada con soporte BIOS${NC}"

# Método 3: Copia bit-a-bit con modificaciones
else
    echo -e "${YELLOW}  Usando método alternativo...${NC}"
    
    # Crear ISO básica
    sudo mkisofs -o "$OUTPUT_ISO" \
        -V "DOS_VBOX" \
        -J -R \
        alpine_custom/ 2>&1 | grep -E "done|Writing" || {
        echo -e "${RED}Error creando ISO${NC}"
        exit 1
    }
    
    echo -e "${YELLOW}  ⚠ ISO creada sin boot (solo para pruebas)${NC}"
fi

if [ -f "$OUTPUT_ISO" ]; then
    SIZE=$(du -h "$OUTPUT_ISO" | cut -f1)
    echo -e "${GREEN}  ✓ Archivo: $OUTPUT_ISO ($SIZE)${NC}"
else
    echo -e "${RED}  ✗ No se pudo crear la ISO${NC}"
    exit 1
fi

echo ""

# ========================================
# PASO 9: CREAR SCRIPTS DE VIRTUALBOX
# ========================================

echo -e "${YELLOW}[9/9]${NC} Creando scripts de VirtualBox..."

# Script para crear VM individual
cat > create_vm.sh << 'EOFVM'
#!/bin/bash

VM_NAME="DOS_Node_${1:-1}"
ISO="dos_virtualbox.iso"

if [ ! -f "$ISO" ]; then
    echo "Error: No se encuentra $ISO"
    exit 1
fi

echo "Creando VM: $VM_NAME"

# Verificar si ya existe
if VBoxManage showvminfo "$VM_NAME" &>/dev/null; then
    echo "VM ya existe, eliminando..."
    VBoxManage unregistervm "$VM_NAME" --delete 2>/dev/null || true
fi

# Crear VM
VBoxManage createvm --name "$VM_NAME" --ostype Linux_64 --register

# Configurar hardware
VBoxManage modifyvm "$VM_NAME" \
    --memory 1024 \
    --cpus 2 \
    --vram 16 \
    --boot1 dvd \
    --boot2 none \
    --boot3 none \
    --boot4 none \
    --audio none \
    --usb off

# Configurar red en modo Bridge
BRIDGE_ADAPTER=$(VBoxManage list bridgedifs | grep "^Name:" | head -1 | cut -d: -f2 | xargs)
VBoxManage modifyvm "$VM_NAME" \
    --nic1 bridged \
    --bridgeadapter1 "$BRIDGE_ADAPTER"

# Crear controlador de almacenamiento
VBoxManage storagectl "$VM_NAME" --name "IDE" --add ide

# Adjuntar ISO
VBoxManage storageattach "$VM_NAME" \
    --storagectl "IDE" \
    --port 0 \
    --device 0 \
    --type dvddrive \
    --medium "$ISO"

echo "✅ VM '$VM_NAME' creada"
echo ""
echo "Para iniciar: VBoxManage startvm '$VM_NAME'"
EOFVM
chmod +x create_vm.sh

# Script para crear cluster
cat > start_cluster.sh << 'EOFCLUSTER'
#!/bin/bash

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  Creando cluster de 3 nodos                               ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

for i in 1 2 3; do
    echo "[Nodo $i] Creando VM..."
    ./create_vm.sh $i
    
    echo "[Nodo $i] Iniciando..."
    VBoxManage startvm "DOS_Node_$i" --type gui &
    
    sleep 2
done

echo ""
echo "✅ Cluster iniciado"
echo "Espera ~30 segundos para que los nodos se descubran"
echo ""
echo "Para detener: ./stop_cluster.sh"
EOFCLUSTER
chmod +x start_cluster.sh

# Script para detener cluster
cat > stop_cluster.sh << 'EOFSTOP'
#!/bin/bash

echo "Deteniendo cluster..."

for i in 1 2 3; do
    VM_NAME="DOS_Node_$i"
    if VBoxManage showvminfo "$VM_NAME" 2>/dev/null | grep -q "running"; then
        echo "  Deteniendo $VM_NAME..."
        VBoxManage controlvm "$VM_NAME" poweroff 2>/dev/null || true
    fi
done

echo "✅ Cluster detenido"
EOFSTOP
chmod +x stop_cluster.sh

echo -e "${GREEN}  ✓ Scripts creados${NC}\n"

# ========================================
# LIMPIEZA
# ========================================

sudo rm -rf alpine_custom iso_work 2>/dev/null || true

# ========================================
# RESUMEN FINAL
# ========================================

echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║              ✅ ISO GENERADA EXITOSAMENTE                 ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

echo -e "${CYAN}📦 Archivos generados:${NC}"
echo ""
echo "  📀 $OUTPUT_ISO"
if [ -f "$OUTPUT_ISO" ]; then
    SIZE=$(du -h "$OUTPUT_ISO" | cut -f1)
    echo "     Tamaño: $SIZE"
fi
echo "  📜 create_vm.sh - Crear VM individual"
echo "  📜 start_cluster.sh - Crear cluster de 3 nodos"
echo "  📜 stop_cluster.sh - Detener cluster"
echo ""

echo -e "${YELLOW}🚀 PRÓXIMOS PASOS:${NC}"
echo ""
echo "1. Probar con un nodo:"
echo -e "   ${GREEN}./create_vm.sh 1${NC}"
echo -e "   ${GREEN}VBoxManage startvm DOS_Node_1${NC}"
echo ""
echo "2. O crear cluster completo:"
echo -e "   ${GREEN}./start_cluster.sh${NC}"
echo ""
echo "3. Dentro de la VM:"
echo "   - Espera ~20 segundos"
echo "   - Escribe: status"
echo "   - Escribe: nodes (para ver otros nodos)"
echo ""

echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✨ Todo listo para probar el sistema${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""