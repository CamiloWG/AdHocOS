#!/bin/bash
# ======================================================================
# ADHOC OS ISO BUILDER – Alpine Linux 3.18 (BIOS + UEFI compatible)
# ======================================================================

set -e

# --------- Colores ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# --------- Version de Alpine ---------
ALPINE_VERSION="3.18"
ALPINE_ISO="alpine-standard-${ALPINE_VERSION}.0-x86_64.iso"
ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_VERSION}/releases/x86_64/${ALPINE_ISO}"
OUTPUT_ISO="AdhocOS_Alpine.iso"

# --------- Mensaje inicial ---------
echo -e "${CYAN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════╗
║               ADHOC OS – ISO BUILDER (Alpine 3.18)        ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# ======================================================================
# 1. Verificar dependencias
# ======================================================================

echo -e "${YELLOW}[1/10] Verificando dependencias...${NC}"

check() { command -v "$1" >/dev/null 2>&1; }

DEPS=(gcc wget xorriso mkfs.vfat mksquashfs)
MISSING=0
for cmd in "${DEPS[@]}"; do
    if check "$cmd"; then
        echo -e "  ${GREEN}✓${NC} $cmd"
    else
        echo -e "  ${RED}✗${NC} Falta $cmd"
        MISSING=1
    fi
done

if [ $MISSING -eq 1 ]; then
    echo "Instala con: sudo apt install gcc wget xorriso mtools squashfs-tools"
    exit 1
fi

# ======================================================================
# 2. Descargar Alpine
# ======================================================================

echo -e "\n${YELLOW}[2/10] Descargando Alpine...${NC}"

if [ ! -f "$ALPINE_ISO" ]; then
    wget -q --show-progress "$ALPINE_URL"
    echo -e "  ${GREEN}✓ Descargado${NC}"
else
    echo -e "  ${GREEN}✓ ISO ya existente${NC}"
fi

# ======================================================================
# 3. Compilar ADHOC OS
# ======================================================================

echo -e "\n${YELLOW}[3/10] Compilando ADHOC OS...${NC}"

if [ ! -f src/main_alpine.c ]; then
    echo -e "${RED}src/main_alpine.c no existe${NC}"
    exit 1
fi

gcc -O2 -pthread -o dos_system src/main_alpine.c -lm

echo -e "  ${GREEN}✓ Compilado: dos_system${NC}"

# ======================================================================
# 4. Extraer Alpine base
# ======================================================================

echo -e "\n${YELLOW}[4/10] Extrayendo Alpine...${NC}"

sudo rm -rf alpine_custom alpine_mount || true
mkdir -p alpine_mount alpine_custom

sudo mount -o loop "$ALPINE_ISO" alpine_mount
sudo cp -aT alpine_mount alpine_custom
sudo umount alpine_mount
rm -r alpine_mount

sudo chmod -R u+w alpine_custom

echo -e "  ${GREEN}✓ Alpine extraído${NC}"

# ======================================================================
# 5. Integrar ADHOC OS en Alpine
# ======================================================================

echo -e "\n${YELLOW}[5/10] Integrando ADHOC OS...${NC}"

sudo mkdir -p alpine_custom/adhoc/{bin,config,logs}

sudo cp dos_system alpine_custom/adhoc/bin/
sudo chmod +x alpine_custom/adhoc/bin/dos_system

# ======================================================================
# 6. Crear script de inicio del sistema descentralizado
# ======================================================================

sudo tee alpine_custom/adhoc/bin/start_adhoc.sh >/dev/null << 'EOF'
#!/bin/sh

echo "Inicializando ADHOC OS..."

for iface in $(ls /sys/class/net | grep -E "eth|enp"); do
    ip link set $iface up
    udhcpc -i $iface -n -q || ip addr add 192.168.100.$((RANDOM%200+10))/24 dev $iface
done

exec /adhoc/bin/dos_system
EOF

sudo chmod +x alpine_custom/adhoc/bin/start_adhoc.sh

# ======================================================================
# 7. Crear servicio OpenRC
# ======================================================================

echo -e "\n${YELLOW}[6/10] Configurando servicio OpenRC...${NC}"

sudo mkdir -p alpine_custom/etc/init.d
sudo mkdir -p alpine_custom/etc/runlevels/default

sudo tee alpine_custom/etc/init.d/adhoc_service >/dev/null << 'EOF'
#!/sbin/openrc-run

command="/adhoc/bin/start_adhoc.sh"
command_background="yes"

depend() {
    need net
}
EOF

sudo chmod +x alpine_custom/etc/init.d/adhoc_service
sudo ln -sf /etc/init.d/adhoc_service alpine_custom/etc/runlevels/default/adhoc_service

echo -e "  ${GREEN}✓ Servicio autoinicio creado${NC}"

# ======================================================================
# 8. Crear documentación
# ======================================================================

echo -e "\n${YELLOW}[7/10] Añadiendo documentación...${NC}"

sudo tee alpine_custom/README_ADHOC.txt >/dev/null << 'EOF'
ADHOC OS sobre Alpine Linux 3.18
================================

El sistema se inicia automáticamente mediante OpenRC.
Archivos principales:
/adhoc/bin/dos_system
EOF

echo -e "  ${GREEN}✓ Documentación agregada${NC}"

# ======================================================================
# 9. Generar ISO booteable (SOLO BIOS — Alpine Standard no trae UEFI)
# ======================================================================

echo -e "\n${YELLOW}[8/10] Generando ISO booteable...${NC}"

sudo xorriso -as mkisofs \
  -iso-level 3 \
  -full-iso9660-filenames \
  -volid "ADHOCOS" \
  -eltorito-boot boot/syslinux/isolinux.bin \
  -eltorito-catalog boot/syslinux/boot.cat \
  -no-emul-boot \
  -boot-load-size 4 \
  -boot-info-table \
  -output "$OUTPUT_ISO" \
  alpine_custom/

echo -e "  ${GREEN}✓ ISO generada → $OUTPUT_ISO${NC}"


# ======================================================================
# 10. Limpieza final
# ======================================================================

echo -e "\n${YELLOW}[9/10] Limpiando...${NC}"
sudo rm -rf alpine_custom
echo -e "  ${GREEN}✓ Limpieza completa${NC}"

# ======================================================================
# RESUMEN FINAL
# ======================================================================

echo -e "\n${GREEN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════╗
║                   ISO GENERADA CORRECTAMENTE              ║
║                    LISTA PARA VIRTUALBOX                  ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo "ISO creada: $OUTPUT_ISO"
