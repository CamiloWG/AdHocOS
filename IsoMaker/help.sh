#!/bin/bash
cat << 'HELP'
╔═══════════════════════════════════════════════════════════╗
║          HERRAMIENTAS DE SOLUCIÓN ISO                      ║
╚═══════════════════════════════════════════════════════════╝

SCRIPTS DISPONIBLES:

📊 DIAGNÓSTICO:
  ./quick_diagnose.sh       - Diagnóstico rápido (30 seg)
  sudo ./diagnose_iso.sh    - Diagnóstico completo (2 min)

🔧 SOLUCIÓN:
  ./create_iso_fixed.sh     - Crear ISO corregida (5 min)

📖 DOCUMENTACIÓN:
  less README_ISO.md        - Guía principal
  less TROUBLESHOOTING.md   - Solución de problemas

🚀 USO RÁPIDO:

1. Diagnosticar:
   ./quick_diagnose.sh

2. Si hay problemas, corregir:
   ./create_iso_fixed.sh

3. Probar en QEMU:
   ./test_iso.sh

4. Configurar VirtualBox:
   less README_ISO.md
   (Ver sección "PASOS DETALLADOS PARA VIRTUALBOX")

═══════════════════════════════════════════════════════════

¿Necesitas ayuda? Lee README_ISO.md
HELP
