#!/bin/bash

# ========================================
# Crear ISO del SO Descentralizado sobre Alpine Linux
# ========================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   SO Descentralizado sobre Alpine Linux                   ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# ========================================
# 1. DESCARGAR ALPINE LINUX BASE
# ========================================

echo -e "${YELLOW}[1/7]${NC} Descargando Alpine Linux..."

ALPINE_VERSION="3.18"
ALPINE_ISO="alpine-standard-${ALPINE_VERSION}.0-x86_64.iso"
ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_VERSION}/releases/x86_64/${ALPINE_ISO}"

if [ ! -f "$ALPINE_ISO" ]; then
    echo "  Descargando Alpine Linux (esto puede tardar)..."
    wget -q --show-progress "$ALPINE_URL" || {
        echo -e "${RED}Error descargando Alpine${NC}"
        exit 1
    }
fi

echo -e "${GREEN}  ✓${NC} Alpine Linux descargado"

# ========================================
# 2. COMPILAR EL SISTEMA DESCENTRALIZADO
# ========================================

echo -e "${YELLOW}[2/7]${NC} Compilando sistema descentralizado con red..."

# Compilar versión con red real
gcc -Wall -O2 -pthread \
    -o dos_network \
    src/main_network.c \
    -lm -lrt || {
        echo -e "${RED}Error compilando${NC}"
        exit 1
    }

echo -e "${GREEN}  ✓${NC} Sistema compilado"

# ========================================
# 3. EXTRAER ALPINE ISO
# ========================================

echo -e "${YELLOW}[3/7]${NC} Extrayendo Alpine Linux..."

# Montar ISO original
mkdir -p alpine_mount alpine_custom
sudo mount -o loop "$ALPINE_ISO" alpine_mount

# Copiar contenido
sudo cp -a alpine_mount/* alpine_custom/
sudo chmod -R u+w alpine_custom/

sudo umount alpine_mount
rmdir alpine_mount

echo -e "${GREEN}  ✓${NC} Alpine extraído"

# ========================================
# 4. PERSONALIZAR ALPINE
# ========================================

echo -e "${YELLOW}[4/7]${NC} Personalizando Alpine..."

# Crear directorio para nuestro software
sudo mkdir -p alpine_custom/dos

# Copiar binario
sudo cp dos_network alpine_custom/dos/
sudo chmod +x alpine_custom/dos/dos_network

# Crear script de inicio automático
sudo tee alpine_custom/dos/start_dos.sh > /dev/null << 'EOFSTART'
#!/bin/sh

# Script de inicio del SO Descentralizado

echo "=========================================="
echo "  Sistema Operativo Descentralizado"
echo "  Iniciando sobre Alpine Linux"
echo "=========================================="
echo ""

# Configurar red automáticamente
echo "[NETWORK] Configurando interfaces de red..."

# Activar todas las interfaces ethernet
for iface in /sys/class/net/*; do
    iface_name=$(basename $iface)
    if [ "$iface_name" != "lo" ]; then
        echo "  Configurando $iface_name..."
        ip link set $iface_name up
        udhcpc -i $iface_name -n -q &
    fi
done

sleep 3

# Mostrar configuración de red
echo ""
echo "[NETWORK] Configuración actual:"
ip addr show | grep -E "^[0-9]+:|inet "
echo ""

# Iniciar el sistema descentralizado
echo "[SYSTEM] Iniciando SO Descentralizado..."
cd /dos
exec ./dos_network
EOFSTART

sudo chmod +x alpine_custom/dos/start_dos.sh

# Crear servicio systemd (si Alpine usa OpenRC)
sudo tee alpine_custom/dos/dos.init > /dev/null << 'EOFINIT'
#!/sbin/openrc-run

name="Sistema Operativo Descentralizado"
command="/dos/start_dos.sh"
command_background=true

depend() {
    need net
    after firewall
}
EOFINIT

sudo chmod +x alpine_custom/dos/dos.init

# Modificar el arranque para iniciar automáticamente
sudo tee alpine_custom/dos/autostart.sh > /dev/null << 'EOFAUTO'
#!/bin/sh

# Este script se ejecutará automáticamente al arrancar

# Esperar a que el sistema esté listo
sleep 5

# Iniciar SO Descentralizado
/dos/start_dos.sh
EOFAUTO

sudo chmod +x alpine_custom/dos/autostart.sh

# Agregar al inittab de Alpine
echo "" | sudo tee -a alpine_custom/etc/inittab > /dev/null
echo "# Sistema Operativo Descentralizado" | sudo tee -a alpine_custom/etc/inittab > /dev/null
echo "dos::respawn:/dos/autostart.sh" | sudo tee -a alpine_custom/etc/inittab > /dev/null

# Crear archivo de configuración
sudo tee alpine_custom/dos/config.conf > /dev/null << 'EOFCONFIG'
# Configuración del SO Descentralizado

# Red
DISCOVERY_PORT=8888
DATA_PORT=8889
BROADCAST_INTERVAL=5
NODE_TIMEOUT=15

# Sistema
AUTO_START=1
DEBUG_MODE=0

# Aplicaciones
ENABLE_ML_APPS=1
ENABLE_MONITORING=1
EOFCONFIG

echo -e "${GREEN}  ✓${NC} Alpine personalizado"

# ========================================
# 5. CREAR README EN LA ISO
# ========================================

echo -e "${YELLOW}[5/7]${NC} Creando documentación..."

sudo tee alpine_custom/README_DOS.txt > /dev/null << 'EOFREADME'
========================================
Sistema Operativo Descentralizado v1.0
Sobre Alpine Linux
========================================

CARACTERÍSTICAS:
- Red Ad hoc automática
- Descubrimiento de nodos por broadcast
- Scheduler distribuido inteligente
- Tolerancia a fallos
- Machine Learning distribuido

INICIO RÁPIDO:

1. Arrancar desde esta ISO
2. El sistema se iniciará automáticamente
3. Esperar ~30 segundos para descubrimiento de red

COMANDOS DISPONIBLES:

Una vez iniciado, puedes usar:
  status  - Ver estado de la red
  nodes   - Listar nodos activos
  task <descripción> - Crear tarea distribuida
  tasks   - Ver todas las tareas
  help    - Mostrar ayuda
  exit    - Salir

CONFIGURACIÓN MANUAL:

Si necesitas configurar manualmente:

1. Acceder a Alpine:
   usuario: root (sin contraseña en live)

2. Iniciar manualmente:
   /dos/start_dos.sh

3. Ver logs:
   dmesg | tail -50

REQUISITOS DE RED:

- Ethernet o WiFi activa
- Misma subred para todos los nodos
- Puertos 8888 (UDP) y 8889 (TCP) abiertos

FIREWALL:

Si hay problemas de conexión:
  iptables -F
  iptables -P INPUT ACCEPT
  iptables -P FORWARD ACCEPT

APLICACIONES DE EJEMPLO:

El sistema incluye ejemplos de:
- Machine Learning distribuido
- Análisis de datos paralelo
- Procesamiento de imágenes

========================================
Desarrollado para redes Ad hoc
========================================
EOFREADME

echo -e "${GREEN}  ✓${NC} Documentación creada"

# ========================================
# 6. GENERAR ISO PERSONALIZADA
# ========================================

echo -e "${YELLOW}[6/7]${NC} Generando ISO personalizada..."

ISO_NAME="dos_alpine.iso"

# Usar xorriso para crear la ISO
sudo xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -volid "DOS_ALPINE" \
    -eltorito-boot boot/grub/eltorito.img \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    -eltorito-catalog boot/grub/boot.cat \
    -output "$ISO_NAME" \
    alpine_custom/ || {
        echo -e "${RED}Error creando ISO${NC}"
        exit 1
    }

SIZE=$(du -h "$ISO_NAME" | cut -f1)
echo -e "${GREEN}  ✓${NC} ISO creada: $ISO_NAME ($SIZE)"

# ========================================
# 7. CREAR SCRIPTS DE PRUEBA
# ========================================

echo -e "${YELLOW}[7/7]${NC} Creando scripts de prueba..."

# Script para probar en QEMU
cat > test_alpine.sh << 'EOFTEST'
#!/bin/bash

ISO="dos_alpine.iso"

echo "Probando SO Descentralizado en QEMU..."
echo ""
echo "Configuración:"
echo "  - 1 GB RAM"
echo "  - Red: user mode con port forwarding"
echo "  - Puertos: 8888 (UDP), 8889 (TCP)"
echo ""
echo "El sistema iniciará automáticamente en ~30 segundos"
echo ""

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 1024 \
    -smp 2 \
    -enable-kvm \
    -netdev user,id=net0,hostfwd=tcp::8889-:8889,hostfwd=udp::8888-:8888 \
    -device e1000,netdev=net0 \
    -boot d
EOFTEST

chmod +x test_alpine.sh

# Script para crear cluster de VMs
cat > test_cluster.sh << 'EOFCLUSTER'
#!/bin/bash

ISO="dos_alpine.iso"

echo "Creando cluster de 3 nodos..."
echo ""

# Crear red bridge
sudo ip link add name br-dos type bridge 2>/dev/null || true
sudo ip addr add 192.168.100.1/24 dev br-dos 2>/dev/null || true
sudo ip link set br-dos up

for i in 1 2 3; do
    echo "Iniciando Nodo $i..."
    
    # Crear interfaz TAP
    sudo ip tuntap add tap-dos$i mode tap
    sudo ip link set tap-dos$i master br-dos
    sudo ip link set tap-dos$i up
    
    # Iniciar VM
    qemu-system-x86_64 \
        -cdrom "$ISO" \
        -m 768 \
        -smp 1 \
        -netdev tap,id=net$i,ifname=tap-dos$i,script=no,downscript=no \
        -device e1000,netdev=net$i,mac=52:54:00:12:34:0$i \
        -display vnc=:$i \
        -name "DOS_Node_$i" \
        -daemonize
    
    sleep 2
done

echo ""
echo "✅ Cluster iniciado"
echo ""
echo "Conectar con VNC:"
echo "  Nodo 1: localhost:5901"
echo "  Nodo 2: localhost:5902"
echo "  Nodo 3: localhost:5903"
echo ""
echo "Los nodos se descubrirán automáticamente"
echo ""
echo "Para detener: sudo killall qemu-system-x86_64"
EOFCLUSTER

chmod +x test_cluster.sh

echo -e "${GREEN}  ✓${NC} Scripts de prueba creados"

# ========================================
# LIMPIEZA
# ========================================

sudo rm -rf alpine_custom

# ========================================
# RESUMEN
# ========================================

echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║          ✅ ISO SOBRE ALPINE LINUX CREADA                 ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}Archivos generados:${NC}"
echo "  📀 $ISO_NAME ($SIZE)"
echo "  🧪 test_alpine.sh - Probar en QEMU"
echo "  🌐 test_cluster.sh - Cluster de 3 nodos"
echo ""
echo -e "${BLUE}Ventajas de esta ISO:${NC}"
echo "  ✅ Linux completo (Alpine) de base"
echo "  ✅ Configuración de red automática"
echo "  ✅ Inicio automático del SO descentralizado"
echo "  ✅ Todas las herramientas necesarias incluidas"
echo "  ✅ Fácil de debuggear y extender"
echo ""
echo -e "${BLUE}Próximos pasos:${NC}"
echo ""
echo "1. Probar en QEMU:"
echo -e "   ${YELLOW}./test_alpine.sh${NC}"
echo ""
echo "2. Probar cluster de 3 nodos:"
echo -e "   ${YELLOW}./test_cluster.sh${NC}"
echo ""
echo "3. Usar en VirtualBox:"
echo "   - Crear VM Linux 64-bit"
echo "   - Mínimo 512 MB RAM"
echo "   - Montar $ISO_NAME"
echo "   - Red en modo Bridge"
echo ""
echo -e "${GREEN}¡Listo para redes Ad hoc reales!${NC}"