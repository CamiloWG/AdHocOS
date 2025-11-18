#!/bin/bash
echo "=== DIAGNÓSTICO RÁPIDO ==="
echo ""
echo "1. Verificando archivos..."
[ -f "decentralized_os.iso" ] && echo "  ✓ ISO principal" || echo "  ✗ ISO principal no encontrada"
[ -f "decentralized_os_fixed.iso" ] && echo "  ✓ ISO corregida" || echo "  ✗ ISO corregida no encontrada"
[ -f "vmlinuz" ] || [ -f "vmlinuz_fixed" ] && echo "  ✓ Kernel" || echo "  ✗ Kernel no encontrado"
[ -f "initramfs.cpio.gz" ] || [ -f "initramfs_fixed.cpio.gz" ] && echo "  ✓ Initramfs" || echo "  ✗ Initramfs no encontrado"
echo ""
echo "2. Sistema:"
echo "  Host: $(uname -s) $(uname -r)"
echo "  Arch: $(uname -m)"
echo ""
echo "3. VirtualBox:"
if command -v VBoxManage &> /dev/null; then
    echo "  ✓ Instalado: $(VBoxManage --version)"
else
    echo "  ✗ VirtualBox no encontrado"
fi
echo ""
echo "4. QEMU:"
if command -v qemu-system-x86_64 &> /dev/null; then
    echo "  ✓ Instalado: $(qemu-system-x86_64 --version | head -1)"
else
    echo "  ✗ QEMU no encontrado (opcional)"
fi
echo ""
echo "Para diagnóstico completo, ejecuta: ./diagnose_iso.sh"
