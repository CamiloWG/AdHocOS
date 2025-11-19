#!/bin/bash

# ========================================
# Script CORREGIDO para crear ISO del SO Descentralizado
# Maneja correctamente Alpine Linux OpenRC
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
║   Para VirtualBox con Red Ad hoc (CORREGIDO)            ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}\n"

# ========================================
# PASO 1: VERIFICAR DEPENDENCIAS
# ========================================

echo -e "${YELLOW}[1/10]${NC} Verificando dependencias..."

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

if [ $MISSING -eq 1 ]; then
    echo -e "${RED}Faltan dependencias. Instala:${NC}"
    echo "  Ubuntu/Debian: sudo apt-get install build-essential wget xorriso"
    echo "  Fedora: sudo dnf install gcc wget xorriso"
    exit 1
fi

echo -e "${GREEN}  ✓ Todas las dependencias presentes${NC}\n"

# ========================================
# PASO 2: DESCARGAR ALPINE LINUX
# ========================================

echo -e "${YELLOW}[2/10]${NC} Descargando Alpine Linux base..."

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

echo -e "${YELLOW}[3/10]${NC} Compilando Sistema Operativo Descentralizado..."

if [ ! -f "src/main_alpine.c" ]; then
    echo -e "${RED}Error: No se encuentra src/main_alpine.c${NC}"
    echo "Asegúrate de estar en el directorio correcto"
    exit 1
fi

echo "  Compilando con optimizaciones..."
gcc -Wall -Wextra -O2 -pthread \
    -o dos_system \
    src/main_alpine.c \
    -lm -lrt || {
        echo -e "${RED}Error en compilación${NC}"
        exit 1
    }

echo -e "${GREEN}  ✓ Sistema compilado: dos_system${NC}"
echo "    Tamaño: $(du -h dos_system | cut -f1)"
echo ""

# ========================================
# PASO 4: EXTRAER ALPINE (CORREGIDO)
# ========================================

echo -e "${YELLOW}[4/10]${NC} Extrayendo Alpine Linux..."

# Limpiar si existe
sudo rm -rf alpine_mount alpine_custom 2>/dev/null || true

mkdir -p alpine_mount alpine_custom

echo "  Montando ISO..."
sudo mount -o loop "$ALPINE_ISO" alpine_mount || {
    echo -e "${RED}Error montando ISO${NC}"
    exit 1
}

echo "  Copiando archivos (esto puede tardar)..."
sudo cp -a alpine_mount/* alpine_custom/ || {
    echo -e "${RED}Error copiando archivos${NC}"
    sudo umount alpine_mount 2>/dev/null
    exit 1
}

sudo chmod -R u+w alpine_custom/

sudo umount alpine_mount
rmdir alpine_mount

# VERIFICAR estructura de Alpine
echo "  Verificando estructura de Alpine..."
if [ ! -d "alpine_custom/boot" ]; then
    echo -e "${RED}Error: Estructura de Alpine inválida${NC}"
    exit 1
fi

echo -e "${GREEN}  ✓ Alpine extraído correctamente${NC}\n"

# ========================================
# PASO 5: PERSONALIZAR ALPINE (CORREGIDO)
# ========================================

echo -e "${YELLOW}[5/10]${NC} Personalizando Alpine Linux..."

# Crear directorio para nuestro sistema
sudo mkdir -p alpine_custom/dos/{bin,config,logs}

# Copiar el binario
sudo cp dos_system alpine_custom/dos/bin/
sudo chmod +x alpine_custom/dos/bin/dos_system

# Crear script de configuración de red
sudo tee alpine_custom/dos/bin/setup_network.sh > /dev/null << 'EOFNET'
#!/bin/sh

echo "[NETWORK] Configurando interfaces de red..."

# Configurar loopback
ip link set lo up
ip addr add 127.0.0.1/8 dev lo

# Buscar y configurar todas las interfaces ethernet
for iface in $(ls /sys/class/net/ 2>/dev/null | grep -E '^eth|^enp' || echo ""); do
    if [ -n "$iface" ]; then
        echo "  Configurando $iface..."
        ip link set $iface up 2>/dev/null
        
        # Intentar DHCP primero
        timeout 5 udhcpc -i $iface -n -q 2>/dev/null || {
            # Si DHCP falla, asignar IP estática
            ip addr add 192.168.100.$((RANDOM % 200 + 10))/24 dev $iface 2>/dev/null
        }
    fi
done

echo "[NETWORK] Configuración de red completada"
echo ""
echo "Interfaces activas:"
ip addr show 2>/dev/null | grep -E "^[0-9]+:|inet " | grep -v "inet 127" || echo "  (verificando...)"
echo ""
EOFNET

sudo chmod +x alpine_custom/dos/bin/setup_network.sh

# Crear script principal de inicio
sudo tee alpine_custom/dos/bin/start_dos.sh > /dev/null << 'EOFSTART'
#!/bin/sh

clear

cat << 'BANNER'
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║      SISTEMA OPERATIVO DESCENTRALIZADO v1.0              ║
║      Iniciando sobre Alpine Linux                        ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝

BANNER

echo ""
echo "Inicializando sistema..."
echo ""

# Configurar red
/dos/bin/setup_network.sh

# Mostrar información del nodo
echo "═══════════════════════════════════════════════════════════"
echo "INFORMACIÓN DEL NODO:"
echo "═══════════════════════════════════════════════════════════"
echo "Hostname: $(hostname)"
echo "Sistema: Alpine Linux $(cat /etc/alpine-release 2>/dev/null || echo 'N/A')"
echo "Kernel: $(uname -r)"
echo "Arquitectura: $(uname -m)"
echo ""

# Advertencia sobre red
echo "⚠️  IMPORTANTE:"
echo "   Asegúrate de que la red de VirtualBox esté en modo"
echo "   'Bridge' o 'Red Interna' para que los nodos se"
echo "   puedan descubrir entre sí."
echo ""
echo "   Puertos usados: 8888 (UDP), 8889 (TCP)"
echo ""

# Esperar un momento
sleep 2

# Iniciar el sistema operativo descentralizado
echo "Iniciando SO Descentralizado..."
echo ""

cd /dos
exec /dos/bin/dos_system
EOFSTART

sudo chmod +x alpine_custom/dos/bin/start_dos.sh

# ========================================
# CONFIGURAR AUTO-INICIO (MÉTODO CORRECTO PARA ALPINE)
# ========================================

echo "  Configurando auto-inicio con OpenRC..."

# Crear servicio OpenRC en lugar de modificar inittab
sudo mkdir -p alpine_custom/etc/init.d
sudo mkdir -p alpine_custom/etc/runlevels/default

sudo tee alpine_custom/etc/init.d/dos > /dev/null << 'EOFINIT'
#!/sbin/openrc-run

name="Sistema Operativo Descentralizado"
description="Sistema Operativo Descentralizado para redes Ad hoc"
command="/dos/bin/start_dos.sh"
command_background="no"
pidfile="/run/dos.pid"

depend() {
    need net localmount
    after firewall
}

start_pre() {
    # Asegurar que los directorios existen
    checkpath --directory --mode 0755 /dos/logs
}
EOFINIT

sudo chmod +x alpine_custom/etc/init.d/dos

# ALTERNATIVA: Usar /etc/local.d/ (más simple y compatible)
echo "  Configurando inicio automático con local.d..."

sudo mkdir -p alpine_custom/etc/local.d

sudo tee alpine_custom/etc/local.d/dos.start > /dev/null << 'EOFLOCAL'
#!/bin/sh
# Auto-inicio del Sistema Operativo Descentralizado

# Esperar a que la red esté lista
sleep 3

# Iniciar en background si queremos que continue el boot
# O en foreground si queremos que tome control
/dos/bin/start_dos.sh &
EOFLOCAL

sudo chmod +x alpine_custom/etc/local.d/dos.start

# ALTERNATIVA 2: Modificar /etc/inittab SOLO SI EXISTE
if [ -f "alpine_custom/etc/inittab" ]; then
    echo "  Modificando inittab existente..."
    sudo tee -a alpine_custom/etc/inittab > /dev/null << 'EOFINIT2'

# Sistema Operativo Descentralizado
dos::respawn:/dos/bin/start_dos.sh
EOFINIT2
else
    echo "  (inittab no existe, usando local.d)"
fi

# Crear configuración
sudo tee alpine_custom/dos/config/dos.conf > /dev/null << 'EOFCONFIG'
# Configuración del Sistema Operativo Descentralizado

[Network]
DISCOVERY_PORT=8888
DATA_PORT=8889
BROADCAST_INTERVAL=5
NODE_TIMEOUT=15

[System]
MAX_NODES=100
MAX_TASKS=1000
AUTO_START=true
DEBUG_MODE=false

[Scheduler]
ALGORITHM=intelligent
LOAD_BALANCING=true
REPUTATION_ENABLED=true

[Memory]
SHARED_MEMORY_SIZE=1GB
REPLICATION_FACTOR=3
CACHE_SIZE=256MB

[Logging]
LOG_LEVEL=INFO
LOG_FILE=/dos/logs/system.log
EOFCONFIG

echo -e "${GREEN}  ✓ Alpine personalizado correctamente${NC}\n"

# ========================================
# PASO 6: DOCUMENTACIÓN
# ========================================

echo -e "${YELLOW}[6/10]${NC} Creando documentación..."

sudo tee alpine_custom/README_DOS.txt > /dev/null << 'EOFREADME'
╔═══════════════════════════════════════════════════════════╗
║   SISTEMA OPERATIVO DESCENTRALIZADO - GUÍA RÁPIDA        ║
╚═══════════════════════════════════════════════════════════╝

INICIO RÁPIDO:
1. Arrancar desde esta ISO
2. El sistema se iniciará automáticamente (~30 segundos)
3. La red se configurará por DHCP automáticamente
4. Los nodos se descubrirán mediante broadcast UDP

COMANDOS DISPONIBLES:
  status  - Ver estado completo (nodos, tareas, sistema)
  nodes   - Listar nodos activos en la red
  task <descripción> - Crear tarea distribuida
  tasks   - Ver todas las tareas
  help    - Mostrar ayuda completa
  exit    - Salir del sistema

CONFIGURACIÓN DE VIRTUALBOX:
Para que funcione la red Ad hoc:

1. MODO BRIDGE (Recomendado):
   VM → Settings → Network → Adapter 1
   - Attached to: Bridged Adapter
   - Name: (Tu interfaz de red física)
   
2. MODO RED INTERNA (Para pruebas locales):
   VM → Settings → Network → Adapter 1
   - Attached to: Internal Network
   - Name: dos_network (mismo en todas las VMs)

3. NO USAR NAT - Los nodos no se verán entre sí

PUERTOS UTILIZADOS:
- 8888/UDP: Descubrimiento de nodos (Broadcast)
- 8889/TCP: Transferencia de datos entre nodos

SOLUCIÓN DE PROBLEMAS:
- Si no aparece el sistema: Espera 30 segundos
- Si no ve otros nodos: Verifica configuración de red
- Si firewall bloquea: Ejecuta en Alpine:
    iptables -F
    iptables -P INPUT ACCEPT

ACCESO MANUAL (Si necesario):
- Usuario: root (sin contraseña)
- Iniciar manualmente: /dos/bin/start_dos.sh
- Ver logs: dmesg | tail -50

═══════════════════════════════════════════════════════════
Sistema desarrollado para redes Ad hoc
═══════════════════════════════════════════════════════════
EOFREADME

echo -e "${GREEN}  ✓ Documentación creada${NC}\n"

# ========================================
# PASO 7: GENERAR ISO
# ========================================

echo -e "${YELLOW}[7/10]${NC} Generando imagen ISO..."

# Verificar que GRUB existe
if [ ! -d "alpine_custom/boot/grub" ]; then
    echo -e "${RED}Error: No se encuentra boot/grub en Alpine${NC}"
    echo "La ISO de Alpine puede estar corrupta"
    exit 1
fi

sudo xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -volid "DOS_VBOX" \
    -eltorito-boot boot/grub/eltorito.img \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    -eltorito-catalog boot/grub/boot.cat \
    -output "$OUTPUT_ISO" \
    alpine_custom/ 2>&1 | grep -v "NOTE" || {
        echo -e "${RED}Error creando ISO${NC}"
        exit 1
    }

SIZE=$(du -h "$OUTPUT_ISO" | cut -f1)
echo -e "${GREEN}  ✓ ISO creada: $OUTPUT_ISO ($SIZE)${NC}\n"

# ========================================
# PASO 8: SCRIPTS DE VIRTUALBOX
# ========================================

echo -e "${YELLOW}[8/10]${NC} Creando scripts para VirtualBox..."

cat > create_vm_vbox.sh << 'EOFVM'
#!/bin/bash

VM_NAME="DOS_Node_$1"
ISO="dos_virtualbox.iso"

if [ -z "$1" ]; then
    echo "Uso: $0 <numero_nodo>"
    echo "Ejemplo: $0 1"
    exit 1
fi

if [ ! -f "$ISO" ]; then
    echo "Error: No se encuentra $ISO"
    exit 1
fi

echo "Creando VM: $VM_NAME"

VBoxManage createvm --name "$VM_NAME" --ostype Linux_64 --register

VBoxManage modifyvm "$VM_NAME" \
    --memory 1024 \
    --cpus 2 \
    --vram 16 \
    --boot1 dvd \
    --boot2 disk \
    --boot3 none \
    --boot4 none \
    --audio none \
    --usb off

VBoxManage modifyvm "$VM_NAME" \
    --nic1 bridged \
    --bridgeadapter1 "$(VBoxManage list bridgedifs | grep ^Name | head -1 | cut -d: -f2 | xargs)"

VBoxManage storagectl "$VM_NAME" --name "IDE" --add ide

VBoxManage storageattach "$VM_NAME" \
    --storagectl "IDE" \
    --port 0 \
    --device 0 \
    --type dvddrive \
    --medium "$ISO"

echo "✅ VM '$VM_NAME' creada"
echo ""
echo "Para iniciar:"
echo "  VBoxManage startvm '$VM_NAME' --type gui"
EOFVM

chmod +x create_vm_vbox.sh

cat > start_cluster_vbox.sh << 'EOFCLUSTER'
#!/bin/bash

echo "Creando cluster de 3 nodos..."
echo ""

for i in 1 2 3; do
    VM_NAME="DOS_Node_$i"
    
    if ! VBoxManage showvminfo "$VM_NAME" &>/dev/null; then
        echo "Creando VM $i..."
        ./create_vm_vbox.sh $i
    fi
    
    echo "Iniciando Nodo $i..."
    VBoxManage startvm "$VM_NAME" --type gui &
    
    sleep 3
done

echo ""
echo "✅ Cluster iniciado"
echo "Espera ~30 segundos para que los nodos se descubran"
EOFCLUSTER

chmod +x start_cluster_vbox.sh

cat > stop_cluster_vbox.sh << 'EOFSTOP'
#!/bin/bash

echo "Deteniendo cluster..."

for i in 1 2 3; do
    VM_NAME="DOS_Node_$i"
    if VBoxManage showvminfo "$VM_NAME" 2>/dev/null | grep -q "running"; then
        echo "Deteniendo Nodo $i..."
        VBoxManage controlvm "$VM_NAME" poweroff
    fi
done

echo "✅ Cluster detenido"
EOFSTOP

chmod +x stop_cluster_vbox.sh

echo -e "${GREEN}  ✓ Scripts de VirtualBox creados${NC}\n"

# ========================================
# PASO 9: GUÍA
# ========================================

echo -e "${YELLOW}[9/10]${NC} Creando guía de uso..."

cat > GUIA_RAPIDA.txt << 'EOFGUIA'
═══════════════════════════════════════════════════════════
GUÍA RÁPIDA - SISTEMA OPERATIVO DESCENTRALIZADO
═══════════════════════════════════════════════════════════

🚀 INICIO RÁPIDO:

1. Crear VMs automáticamente:
   ./start_cluster_vbox.sh

2. O crear manualmente:
   ./create_vm_vbox.sh 1
   VBoxManage startvm DOS_Node_1 --type gui

3. Esperar ~30 segundos mientras carga

4. Usar comandos:
   > status  (ver estado)
   > nodes   (ver nodos conectados)
   > task Mi tarea  (crear tarea)
   > tasks   (ver todas las tareas)

═══════════════════════════════════════════════════════════

⚠️ IMPORTANTE - CONFIGURACIÓN DE RED:

VirtualBox → VM → Settings → Network

OPCIÓN A - Red entre VMs en mismo PC:
  • Attached to: Internal Network
  • Name: dos_network (MISMO en todas las VMs)

OPCIÓN B - Red real (VMs en diferentes PCs):
  • Attached to: Bridged Adapter
  • Name: Tu interfaz de red (eth0, wlan0, etc.)

❌ NO USAR NAT (no funciona para Ad hoc)

═══════════════════════════════════════════════════════════

📝 PRUEBA DE FUNCIONAMIENTO:

En VM 1:
> nodes
(esperar 10-15 segundos)
> nodes
(deberías ver VM 2 y VM 3)

En VM 2:
> task Prueba desde VM2

En VM 1:
> tasks
(deberías ver la tarea de VM 2)

═══════════════════════════════════════════════════════════

🐛 SOLUCIÓN DE PROBLEMAS:

• Pantalla negra: Espera 30 segundos
• No ve otros nodos: Verifica configuración de red
• ISO no bootea: Verifica orden de arranque (CD primero)
• VM lenta: Aumenta RAM a 1024 MB

═══════════════════════════════════════════════════════════
EOFGUIA

echo -e "${GREEN}  ✓ Guía creada${NC}\n"

# ========================================
# PASO 10: LIMPIEZA
# ========================================

echo -e "${YELLOW}[10/10]${NC} Limpiando archivos temporales..."

sudo rm -rf alpine_custom alpine_mount 2>/dev/null || true

echo -e "${GREEN}  ✓ Limpieza completada${NC}\n"

# ========================================
# RESUMEN
# ========================================

echo -e "${GREEN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════╗
║              ✅ GENERACIÓN COMPLETA                       ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo -e "${BLUE}📦 Archivos generados:${NC}"
echo ""
echo "  📀 $OUTPUT_ISO ($SIZE)"
echo "  📜 create_vm_vbox.sh"
echo "  📜 start_cluster_vbox.sh"
echo "  📜 stop_cluster_vbox.sh"
echo "  📖 GUIA_RAPIDA.txt"
echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}🚀 PRÓXIMOS PASOS:${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "1. Crear y probar cluster:"
echo -e "   ${GREEN}./start_cluster_vbox.sh${NC}"
echo ""
echo "2. Ver guía rápida:"
echo -e "   ${GREEN}cat GUIA_RAPIDA.txt${NC}"
echo ""
echo -e "${GREEN}✨ Sistema listo para VirtualBox${NC}"
echo ""