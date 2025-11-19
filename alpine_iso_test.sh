#!/bin/bash
# ============================================================
# ADHOC OS BUILDER — Alpine Linux 3.18 100% compatible
# Auto-inicio usando OpenRC (ya no existe /etc/inittab)
# ============================================================

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
OUTPUT_ISO="alpine_adhoc.iso"

echo -e "${CYAN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════╗
║   GENERADOR ISO – ADHOC OS SOBRE ALPINE 3.18             ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}\n"

# ============================================================
# 1. VERIFICAR DEPENDENCIAS
# ============================================================

check() { command -v "$1" &>/dev/null; }

echo -e "${YELLOW}[1/10] Verificando dependencias...${NC}"

MISSING=0
for cmd in gcc wget xorriso; do
    if check "$cmd"; then echo -e "  ${GREEN}✓${NC} $cmd"; else
        echo -e "  ${RED}✗${NC} Falta $cmd"; MISSING=1; fi
done

[ $MISSING -eq 1 ] && echo "Instala lo necesario." && exit 1

echo -e "${GREEN}✓ Dependencias verificadas${NC}\n"

# ============================================================
# 2. DESCARGAR ALPINE
# ============================================================

echo -e "${YELLOW}[2/10] Descargando Alpine...${NC}"

if [ ! -f "$ALPINE_ISO" ]; then
    wget -q --show-progress "$ALPINE_URL"
fi

echo -e "${GREEN}✓ Alpine descargado${NC}\n"

# ============================================================
# 3. COMPILAR ADHOC OS
# ============================================================

echo -e "${YELLOW}[3/10] Compilando Sistema Descentralizado...${NC}"

if [ ! -f src/main_alpine.c ]; then
    echo -e "${RED}No existe src/main_alpine.c${NC}"
    exit 1
fi

gcc -O2 -pthread -o dos_system src/main_alpine.c -lm

echo -e "${GREEN}✓ Compilado: dos_system${NC}\n"

# ============================================================
# 4. EXTRAER ALPINE
# ============================================================

echo -e "${YELLOW}[4/10] Extrayendo Alpine...${NC}"

sudo rm -rf alpine_mount alpine_custom || true
mkdir -p alpine_mount alpine_custom

sudo mount -o loop "$ALPINE_ISO" alpine_mount
sudo cp -a alpine_mount/* alpine_custom/
sudo chmod -R u+w alpine_custom/

sudo umount alpine_mount
rmdir alpine_mount

echo -e "${GREEN}✓ Alpine extraído${NC}\n"

# ============================================================
# 5. PERSONALIZACIÓN DEL SISTEMA
# ============================================================

echo -e "${YELLOW}[5/10] Personalizando Alpine...${NC}"

# --- Estructura /dos ---
sudo mkdir -p alpine_custom/dos/{bin,config,logs}

# Copiar binario
sudo cp dos_system alpine_custom/dos/bin/
sudo chmod +x alpine_custom/dos/bin/dos_system

# ============================================================
# Servicio OpenRC (nuevo método, reemplaza inittab)
# ============================================================

echo "Creando servicio OpenRC..."

sudo mkdir -p alpine_custom/etc/init.d
sudo mkdir -p alpine_custom/etc/runlevels/default

sudo tee alpine_custom/etc/init.d/dos_service >/dev/null << 'EOF'
#!/sbin/openrc-run
command="/dos/bin/start_dos.sh"
command_background="yes"

depend() {
    need net
}
EOF

sudo chmod +x alpine_custom/etc/init.d/dos_service
sudo ln -sf /etc/init.d/dos_service alpine_custom/etc/runlevels/default/dos_service

# ============================================================
# Script de inicio
# ============================================================

sudo tee alpine_custom/dos/bin/start_dos.sh >/dev/null << 'EOF'
#!/bin/sh
clear

echo "Iniciando ADHOC OS..."

# Configurar red mínima
for iface in $(ls /sys/class/net | grep -E 'eth|enp'); do
    ip link set $iface up
    udhcpc -i $iface -n -q || ip addr add 192.168.100.$((RANDOM%200+10))/24 dev $iface
done

exec /dos/bin/dos_system
EOF

sudo chmod +x alpine_custom/dos/bin/start_dos.sh

# ============================================================
# Config archivo
# ============================================================

sudo tee alpine_custom/dos/config/dos.conf >/dev/null << 'EOF'
[Network]
DISCOVERY_PORT=8888
DATA_PORT=8889

[System]
DEBUG=false
EOF

echo -e "${GREEN}✓ Alpine Personalizado${NC}\n"

# ============================================================
# 6. DOCUMENTACIÓN
# ============================================================

echo -e "${YELLOW}[6/10] Documentación...${NC}"

sudo tee alpine_custom/README_DOS.txt >/dev/null << 'EOF'
ADHOC OS sobre Alpine Linux 3.18
================================
Inicio automático mediante OpenRC.
EOF

echo -e "${GREEN}✓ Documentación lista${NC}\n"

# ============================================================
# 7. GENERAR ISO
# ============================================================

echo -e "${YELLOW}[7/10] Generando ISO...${NC}"

sudo xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -volid "ADHOCOS" \
    -output "$OUTPUT_ISO" \
    alpine_custom/

echo -e "${GREEN}✓ ISO creada: $OUTPUT_ISO${NC}\n"

# ============================================================
# 8. SCRIPTS VIRTUALBOX
# ============================================================

echo -e "${YELLOW}[8/10] Scripts VirtualBox...${NC}"

cat > create_vm_vbox.sh << 'EOF'
#!/bin/bash
VM_NAME="AdhocNode_$1"
ISO="alpine_adhoc.iso"

VBoxManage createvm --name "$VM_NAME" --ostype Linux_64 --register
VBoxManage modifyvm "$VM_NAME" --memory 1024 --cpus 2 --nic1 bridged
VBoxManage storagectl "$VM_NAME" --name "IDE" --add ide
VBoxManage storageattach "$VM_NAME" --storagectl "IDE" --port 0 --device 0 --type dvddrive --medium "$ISO"
EOF

chmod +x create_vm_vbox.sh

echo -e "${GREEN}✓ Scripts VBOX creados${NC}\n"

# ============================================================
# 10. LIMPIEZA
# ============================================================

echo -e "${YELLOW}[9/10] Limpiando...${NC}"
sudo rm -rf alpine_custom
echo -e "${GREEN}✓ Limpieza completa${NC}\n"

# ============================================================
# RESUMEN
# ============================================================

echo -e "${GREEN}ISO GENERADA EXITOSAMENTE → $OUTPUT_ISO${NC}"
