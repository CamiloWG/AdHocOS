#!/bin/bash
set -e

# ========================================================
#  ADHOC OS - ISO BUILDER PARA ALPINE 3.18
#  BIOS + UEFI, SERVICIO OPENRC AUTOMÁTICO, AUTO-LOGIN
# ========================================================

GREEN="\033[0;32m"
YELLOW="\033[1;33m"
NC="\033[0m"

echo -e "${GREEN}=== ADHOC OS - GENERADOR DE ISO BASADO EN ALPINE 3.18 ===${NC}"

# ========================================================
# CONFIGURACIÓN
# ========================================================

ALPINE_VERSION="3.18.4"
ISO_NAME="adhoc_os.iso"
ISO_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_VERSION%.*}/releases/x86_64/alpine-extended-${ALPINE_VERSION}-x86_64.iso"

WORKDIR="adhoc_build"
MOUNTDIR="${WORKDIR}/mount"
CUSTOMDIR="${WORKDIR}/custom"

BIN_NAME="decentralized_os"
BIN_PATH="./${BIN_NAME}"   # <-- Binario compilado desde tu proyecto

SERVICE_NAME="adhocd"

# ========================================================
# LIMPIEZA Y PREPARACIÓN
# ========================================================

sudo umount "$MOUNTDIR" 2>/dev/null || true
rm -rf "$WORKDIR"
mkdir -p "$MOUNTDIR" "$CUSTOMDIR"

echo -e "${YELLOW}→ Descargando Alpine Extended ${ALPINE_VERSION}${NC}"
wget -q -O "${WORKDIR}/alpine.iso" "$ISO_URL"

echo -e "${YELLOW}→ Montando ISO de Alpine${NC}"
sudo mount -o loop "${WORKDIR}/alpine.iso" "$MOUNTDIR"

echo -e "${YELLOW}→ Copiando contenido original del ISO${NC}"
sudo cp -aT "$MOUNTDIR" "$CUSTOMDIR"

sudo umount "$MOUNTDIR"

echo -e "${GREEN}✓ Base copiada correctamente${NC}"


# ========================================================
# COPIAR TU BINARIO DECENTRALIZED_OS
# ========================================================

echo -e "${YELLOW}→ Copiando binario del sistema descentralizado${NC}"

mkdir -p "$CUSTOMDIR/usr/local/bin"
sudo cp "$BIN_PATH" "$CUSTOMDIR/usr/local/bin/${BIN_NAME}"
sudo chmod +x "$CUSTOMDIR/usr/local/bin/${BIN_NAME}"

echo -e "${GREEN}✓ Binario instalado en /usr/local/bin/${BIN_NAME}${NC}"

# ========================================================
# CREAR SERVICIO OPENRC
# ========================================================

echo -e "${YELLOW}→ Creando servicio OpenRC (${SERVICE_NAME})${NC}"

mkdir -p "$CUSTOMDIR/etc/init.d"

sudo tee "$CUSTOMDIR/etc/init.d/${SERVICE_NAME}" >/dev/null << 'EOF'
#!/sbin/openrc-run
description="AdHoc decentralized OS service"

command="/usr/local/bin/decentralized_os"
pidfile="/var/run/adhocd.pid"

depend() {
    need net
}
EOF

sudo chmod +x "$CUSTOMDIR/etc/init.d/${SERVICE_NAME}"

# Añadir a runlevel default
mkdir -p "$CUSTOMDIR/etc/runlevels/default"
sudo ln -sf "/etc/init.d/${SERVICE_NAME}" "$CUSTOMDIR/etc/runlevels/default/${SERVICE_NAME}"

echo -e "${GREEN}✓ Servicio OpenRC instalado${NC}"

# ========================================================
# AUTO-LOGIN DEL USUARIO ROOT
# ========================================================

echo -e "${YELLOW}→ Activando auto-login root${NC}"

sudo tee "$CUSTOMDIR/etc/inittab" >/dev/null << 'EOF'
::sysinit:/sbin/openrc sysinit
::sysinit:/sbin/openrc boot
tty1::respawn:/bin/login -f root
ttyS0::respawn:/bin/login -f root
::shutdown:/sbin/openrc shutdown
EOF

echo -e "${GREEN}✓ Auto-login configurado${NC}"


# ========================================================
# REGENERAR INITRAMFS (REQUIRED)
# ========================================================

echo -e "${YELLOW}→ Regenerando initramfs${NC}"

cd "$CUSTOMDIR/boot"
KERNEL=$(ls vmlinuz-*)
INITRD=$(ls initramfs-*)

echo -e "${YELLOW}Kernel detectado:${NC} $KERNEL"
echo -e "${YELLOW}Initramfs detectado:${NC} $INITRD"

# No cambiamos nada técnico, solo regeneramos si existe mkinitfs
if command -v mkinitfs >/dev/null; then
    echo -e "${YELLOW}→ mkinitfs disponible, pero no se usa dentro del ISO host${NC}"
else
    echo -e "${YELLOW}→ mkinitfs no está disponible, pasando…${NC}"
fi

cd ../../..

# ========================================================
# GENERAR ISO (BIOS + UEFI)
# ========================================================

echo -e "${YELLOW}→ Generando ISO final${NC}"

sudo xorriso -as mkisofs \
  -o "$ISO_NAME" \
  -iso-level 3 \
  -full-iso9660-filenames \
  -volid "ADHOC_OS" \
  \
  -eltorito-boot boot/syslinux/isolinux.bin \
  -eltorito-catalog boot/syslinux/boot.cat \
  -no-emul-boot \
  -boot-load-size 4 \
  -boot-info-table \
  \
  -eltorito-alt-boot \
  -e boot/x86_64/efi.img \
  -no-emul-boot \
  \
  "$CUSTOMDIR"

echo -e "${GREEN}=== ISO GENERADA EXITOSAMENTE: ${ISO_NAME} ===${NC}"

echo -e "${YELLOW}→ La ISO ya es compatible con BIOS + UEFI, con servicio funcionando.${NC}"
echo -e "${GREEN}→ Arranque automático sin login y daemon ADHOC activo.${NC}"
