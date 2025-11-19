#!/bin/bash

# ========================================
# Script COMPLETO para crear ISO del SO Descentralizado
# Sobre Alpine Linux para VirtualBox
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
OUTPUT_ISO="alpine_adhoc.iso"

echo -e "${CYAN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════╗
║                                                           ║
║   GENERADOR DE ISO - SISTEMA OPERATIVO DESCENTRALIZADO   ║
║   Para VirtualBox con Red Ad hoc                         ║
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
    echo "  Descargando $ALPINE_ISO (puede tardar algunos minutos)..."
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

# Verificar que existe el código fuente
if [ ! -f "src/main_alpine.c" ]; then
    echo -e "${RED}Error: No se encuentra src/main_alpine.c${NC}"
    echo "Asegúrate de estar en el directorio correcto"
    exit 1
fi

# Compilar
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
# PASO 4: EXTRAER ALPINE
# ========================================

echo -e "${YELLOW}[4/10]${NC} Extrayendo Alpine Linux..."

# Limpiar si existe
sudo rm -rf alpine_mount alpine_custom 2>/dev/null || true

mkdir -p alpine_mount alpine_custom

sudo mount -o loop "$ALPINE_ISO" alpine_mount
echo "  Copiando archivos..."
sudo cp -a alpine_mount/* alpine_custom/
sudo chmod -R u+w alpine_custom/
sudo umount alpine_mount
rmdir alpine_mount

echo -e "${GREEN}  ✓ Alpine extraído${NC}\n"

# ========================================
# PASO 5: PERSONALIZAR ALPINE
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
for iface in $(ls /sys/class/net/ | grep -E '^eth|^enp'); do
    echo "  Configurando $iface..."
    ip link set $iface up
    
    # Intentar DHCP primero
    timeout 5 udhcpc -i $iface -n -q 2>/dev/null || {
        # Si DHCP falla, asignar IP estática
        ip addr add 192.168.100.$((RANDOM % 200 + 10))/24 dev $iface
    }
done

echo "[NETWORK] Configuración de red completada"
echo ""
echo "Interfaces activas:"
ip addr show | grep -E "^[0-9]+:|inet " | grep -v "inet 127"
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

# Crear configuración de auto-inicio
echo "  Configurando auto-inicio..."

# Modificar inittab para auto-inicio
sudo tee -a alpine_custom/etc/inittab > /dev/null << 'EOFINIT'

# Sistema Operativo Descentralizado
dos::respawn:/dos/bin/start_dos.sh
EOFINIT

# Crear archivo de configuración
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

echo -e "${GREEN}  ✓ Alpine personalizado${NC}\n"

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
   
   ✅ Los nodos en diferentes VMs se verán
   ✅ Pueden estar en diferentes PCs

2. MODO RED INTERNA (Para pruebas locales):
   VM → Settings → Network → Adapter 1
   - Attached to: Internal Network
   - Name: dos_network (mismo en todas las VMs)
   
   ✅ VMs en el mismo host se verán
   ❌ No saldrán a internet

3. NO USAR NAT - Los nodos no se verán entre sí

PUERTOS UTILIZADOS:
- 8888/UDP: Descubrimiento de nodos (Broadcast)
- 8889/TCP: Transferencia de datos entre nodos

ARQUITECTURA:
┌─────────────────────────────────────┐
│  Sistema Operativo Descentralizado  │
│  - Scheduler distribuido            │
│  - Memoria compartida               │
│  - Sincronización                   │
│  - Tolerancia a fallos              │
├─────────────────────────────────────┤
│  Alpine Linux 3.18 (Base)           │
│  - Kernel Linux                     │
│  - Red TCP/IP                       │
│  - Herramientas básicas             │
└─────────────────────────────────────┘

PRUEBA CON MÚLTIPLES NODOS:
1. Crea 2-3 VMs con esta misma ISO
2. Configura todas en Bridge o Red Interna
3. Iníc ialas simultáneamente
4. Usa 'nodes' para ver cuándo se descubren
5. Usa 'task' para distribuir trabajo

SOLUCIÓN DE PROBLEMAS:
- Si no aparece GRUB: Verifica orden de arranque (CD primero)
- Si pantalla negra: Espera 30 seg o selecciona modo Verbose
- Si no ve otros nodos: Verifica configuración de red
- Si firewall bloquea: Ejecuta en Alpine:
    iptables -F
    iptables -P INPUT ACCEPT

ACCESO MANUAL (Si necesario):
- Usuario: root (sin contraseña en live)
- Para iniciar manualmente: /dos/bin/start_dos.sh
- Ver logs: dmesg | tail

CARACTERÍSTICAS:
✅ Descubrimiento automático de nodos
✅ Scheduler inteligente con balanceo de carga
✅ Memoria compartida distribuida
✅ Tolerancia a fallos con recuperación automática
✅ Interfaz de comandos interactiva
✅ Soporte para aplicaciones de Machine Learning

═══════════════════════════════════════════════════════════
Sistema desarrollado para redes Ad hoc
Más información: Ver documentación del proyecto
═══════════════════════════════════════════════════════════
EOFREADME

echo -e "${GREEN}  ✓ Documentación creada${NC}\n"

# ========================================
# PASO 7: GENERAR ISO
# ========================================

echo -e "${YELLOW}[7/10]${NC} Generando imagen ISO..."

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
# PASO 8: CREAR SCRIPTS DE VIRTUALBOX
# ========================================

echo -e "${YELLOW}[8/10]${NC} Creando scripts para VirtualBox..."

# Script para crear VM automáticamente
cat > create_vm_vbox.sh << 'EOFVM'
#!/bin/bash

# Script para crear VM de VirtualBox automáticamente

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

# Crear VM
VBoxManage createvm --name "$VM_NAME" --ostype Linux_64 --register

# Configurar sistema
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

# Configurar red (Bridge - para red real)
VBoxManage modifyvm "$VM_NAME" \
    --nic1 bridged \
    --bridgeadapter1 "$(VBoxManage list bridgedifs | grep ^Name | head -1 | cut -d: -f2 | xargs)"

# Crear controlador de almacenamiento
VBoxManage storagectl "$VM_NAME" --name "IDE" --add ide

# Montar ISO
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
echo ""
echo "O desde la interfaz gráfica de VirtualBox"
EOFVM

chmod +x create_vm_vbox.sh

# Script para iniciar cluster
cat > start_cluster_vbox.sh << 'EOFCLUSTER'
#!/bin/bash

# Iniciar cluster de 3 nodos

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
echo ""
echo "Espera ~30 segundos para que los nodos se descubran"
echo "Usa 'nodes' en cada VM para ver la red"
EOFCLUSTER

chmod +x start_cluster_vbox.sh

# Script para detener cluster
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
# PASO 9: GUÍA DE CONFIGURACIÓN
# ========================================

echo -e "${YELLOW}[9/10]${NC} Creando guía de configuración..."

cat > GUIA_VIRTUALBOX.md << 'EOFGUIA'
# Guía Completa para VirtualBox

## 🚀 INICIO RÁPIDO (Automático)

### Opción 1: Crear VM Automáticamente

```bash
# Crear una VM
./create_vm_vbox.sh 1

# Iniciar cluster completo (3 VMs)
./start_cluster_vbox.sh

# Detener cluster
./stop_cluster_vbox.sh
```

---

## 🔧 CONFIGURACIÓN MANUAL

### Paso 1: Crear Nueva Máquina Virtual

1. Abrir VirtualBox
2. Click en "Nueva" (New)
3. Configurar:
   - **Nombre**: DOS_Node_1
   - **Tipo**: Linux
   - **Versión**: Other Linux (64-bit)
   - Click "Siguiente"

### Paso 2: Memoria RAM

- **Mínimo**: 512 MB
- **Recomendado**: 1024 MB (1 GB)
- Para pruebas con múltiples VMs: 768 MB por VM

### Paso 3: Disco Duro

- Seleccionar: **No agregar disco duro virtual**
- (Bootearemos directamente desde ISO)

### Paso 4: Configurar Sistema

1. Click derecho en la VM → **Configuración**
2. **Sistema** → **Placa Base**:
   - ✅ Habilitar I/O APIC
   - Orden de arranque: **Óptica** primero
3. **Sistema** → **Procesador**:
   - CPUs: **2** (recomendado)
   - ✅ Habilitar PAE/NX

### Paso 5: Configurar Pantalla

- **Pantalla** → **Pantalla**:
  - Memoria de vídeo: **16 MB**
  - Controlador gráfico: **VMSVGA**
  - ❌ NO habilitar aceleración 3D

### Paso 6: Configurar Red ⚠️ MUY IMPORTANTE

**Para Red Ad hoc entre VMs en el MISMO HOST:**

1. **Red** → **Adaptador 1**:
   - ✅ Habilitar adaptador de red
   - **Conectado a**: Red interna (Internal Network)
   - **Nombre**: `dos_network` (mismo nombre en todas las VMs)

**Para Red Ad hoc entre VMs en DIFERENTES HOSTS:**

1. **Red** → **Adaptador 1**:
   - ✅ Habilitar adaptador de red
   - **Conectado a**: Adaptador puente (Bridged Adapter)
   - **Nombre**: Tu interfaz de red física (eth0, wlan0, etc.)

### Paso 7: Montar ISO

1. **Almacenamiento** → **Controlador: IDE**
2. Click en el icono del disco (vacío)
3. Click en el icono del disco azul (derecha)
4. **Choose a disk file...** → Seleccionar `dos_virtualbox.iso`

### Paso 8: Iniciar VM

1. Seleccionar la VM
2. Click en **Iniciar**
3. Esperar ~30 segundos mientras carga
4. Deberías ver el menú de comandos del SO

---

## 🌐 CONFIGURACIÓN DE RED DETALLADA

### Modo 1: Red Interna (VMs en mismo PC)

```
┌────────────────────────────────────────┐
│  Host (Tu PC)                          │
│  ┌──────────┐  ┌──────────┐           │
│  │  VM 1    │  │  VM 2    │           │
│  │  ┌────┐  │  │  ┌────┐  │           │
│  │  │dos │←─┼──┼→ │dos │  │           │
│  │  └────┘  │  │  └────┘  │           │
│  └──────────┘  └──────────┘           │
└────────────────────────────────────────┘
   ↓ Red Interna "dos_network"
   ✅ Se ven entre sí
   ❌ Sin acceso a internet
```

**Configuración:**
- Network → Adapter 1 → Internal Network
- Name: `dos_network` (MISMO en todas)

### Modo 2: Adaptador Puente (Red real)

```
Internet
   ↓
Router (192.168.1.1)
   ├── PC 1 (192.168.1.10)
   │     └── VM 1 (192.168.1.100)
   │
   ├── PC 2 (192.168.1.11)
   │     └── VM 2 (192.168.1.101)
   │
   └── PC 3 (192.168.1.12)
         └── VM 3 (192.168.1.102)
         
✅ Todas las VMs se ven entre sí
✅ Incluso en diferentes PCs
✅ Tienen acceso a internet
```

**Configuración:**
- Network → Adapter 1 → Bridged Adapter
- Name: Tu interfaz física (eth0, wlan0, enp0s3...)

### ❌ NO USAR NAT

Si usas NAT, las VMs NO SE VERÁN entre sí.

---

## 📋 CREAR CLUSTER DE 3 NODOS

### Método Manual:

1. Crear 3 VMs siguiendo los pasos anteriores
2. Nombrarlas: DOS_Node_1, DOS_Node_2, DOS_Node_3
3. IMPORTANTE: Misma configuración de red en todas
4. Montar la MISMA ISO en todas
5. Iniciar las 3 VMs

### Método Automático:

```bash
./start_cluster_vbox.sh
```

### Verificar Red:

En cada VM, ejecuta:
```
> nodes
```

Deberías ver los otros 2 nodos listados.

---

## 🐛 SOLUCIÓN DE PROBLEMAS

### Problema: Pantalla negra

**Solución:**
1. Espera 30 segundos (puede estar cargando)
2. Presiona Enter varias veces
3. En GRUB, selecciona "Modo Verbose"
4. Verifica que la ISO esté montada correctamente

### Problema: "No booteable device"

**Solución:**
1. Configuración → Sistema → Orden de arranque
2. Óptica DEBE estar primero
3. Verificar que la ISO está montada en Almacenamiento

### Problema: No detecta otros nodos

**Solución:**
1. Verifica que TODAS las VMs usan la misma configuración de red
2. Si Red Interna: MISMO nombre de red en todas
3. Si Bridge: Verifica firewall del host
4. Espera 15-20 segundos para descubrimiento
5. Ejecuta `status` para ver estado de red

### Problema: "VT-x is disabled"

**Solución:**
1. Reinicia el PC
2. Entra a BIOS/UEFI (F2, F10, o DEL al inicio)
3. Busca "Virtualization" o "Intel VT-x" o "AMD-V"
4. Habilitarlo
5. Guardar y reiniciar

### Problema: VM muy lenta

**Solución:**
1. Aumentar RAM a 1024 MB
2. Habilitar VT-x/AMD-V en BIOS
3. Sistema → Aceleración → Habilitar VT-x/AMD-V
4. Cerrar otras aplicaciones

---

## 📊 COMANDOS DEL SISTEMA

Una vez dentro del sistema:

```bash
# Ver estado completo
> status

# Ver nodos activos
> nodes

# Crear tarea distribuida
> task Procesar dataset grande

# Ver todas las tareas
> tasks

# Ayuda
> help

# Salir
> exit
```

---

## 🎯 PRUEBA COMPLETA DE RED AD HOC

### Escenario: 3 VMs en el mismo PC

1. **Preparación:**
```bash
# Crear las VMs automáticamente
./create_vm_vbox.sh 1
./create_vm_vbox.sh 2
./create_vm_vbox.sh 3
```

2. **Iniciar:**
```bash
./start_cluster_vbox.sh
```

3. **Verificar en VM 1:**
```
> status
# Debería mostrar 2 nodos activos

> nodes
# Lista: Node 2 y Node 3
```

4. **Crear tarea en VM 1:**
```
> task Calcular fibonacci 1000000
# La tarea se asignará al nodo con menor carga
```

5. **Ver en VM 2:**
```
> tasks
# Debería ver la tarea creada en VM 1
```

**Resultado Esperado:**
- ✅ Las 3 VMs se ven entre sí en ~15 segundos
- ✅ Puedes crear tareas desde cualquier nodo
- ✅ Las tareas se distribuyen inteligentemente

---

## 📚 INFORMACIÓN ADICIONAL

### Puertos Utilizados:
- **8888/UDP**: Descubrimiento de nodos (broadcast)
- **8889/TCP**: Transferencia de datos

### Requisitos del Sistema (por VM):
- **RAM**: Mínimo 512 MB, recomendado 1 GB
- **CPU**: 1 core (mínimo), 2 cores (recomendado)
- **Disco**: No necesario (live boot)
- **Red**: Ethernet virtual

### Arquitectura:
```
┌─────────────────────────────────┐
│ Tu Sistema Operativo           │  ← Tu código
│ Descentralizado                 │
├─────────────────────────────────┤
│ Alpine Linux 3.18               │  ← Sistema base
│ (Kernel + Drivers + Red)        │
└─────────────────────────────────┘
```

### Logs y Debug:
Si necesitas debugging:
1. En Alpine: `dmesg | tail -50`
2. Ver configuración de red: `ip addr`
3. Probar conectividad: `ping <IP_otro_nodo>`

---

## ✅ CHECKLIST

Antes de iniciar, verifica:

- [ ] VirtualBox instalado y actualizado
- [ ] ISO descargada: `dos_virtualbox.iso`
- [ ] VMs creadas con configuración correcta
- [ ] Red configurada (Internal o Bridge)
- [ ] ISO montada en cada VM
- [ ] Orden de arranque: CD/DVD primero
- [ ] Suficiente RAM asignada (1 GB)

---

## 🎓 Para el Proyecto

Este sistema implementa:
- ✅ Descubrimiento automático de nodos (Ad hoc)
- ✅ Scheduler distribuido con balanceo de carga
- ✅ Gestión de memoria compartida
- ✅ Sincronización entre procesos distribuidos
- ✅ Tolerancia a fallos con recuperación
- ✅ Interfaz de comandos interactiva

**Listo para demostración y pruebas en red real.**
EOFGUIA

echo -e "${GREEN}  ✓ Guía completa creada: GUIA_VIRTUALBOX.md${NC}\n"

# ========================================
# PASO 10: LIMPIEZA
# ========================================

echo -e "${YELLOW}[10/10]${NC} Limpiando archivos temporales..."

sudo rm -rf alpine_custom alpine_mount 2>/dev/null || true

echo -e "${GREEN}  ✓ Limpieza completada${NC}\n"

# ========================================
# RESUMEN FINAL
# ========================================

echo -e "${GREEN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════╗
║                  ✅ GENERACIÓN COMPLETA                   ║
╚═══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo -e "${BLUE}📦 Archivos generados:${NC}"
echo ""
echo "  📀 $OUTPUT_ISO ($SIZE)"
echo "     └─ ISO booteable lista para VirtualBox"
echo ""
echo "  📜 Scripts de VirtualBox:"
echo "     ├─ create_vm_vbox.sh    (Crear VM individual)"
echo "     ├─ start_cluster_vbox.sh (Iniciar 3 VMs)"
echo "     └─ stop_cluster_vbox.sh  (Detener cluster)"
echo ""
echo "  📖 Documentación:"
echo "     ├─ GUIA_VIRTUALBOX.md    (Guía paso a paso)"
echo "     └─ README_DOS.txt         (En la ISO)"
echo ""

echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${YELLOW}🚀 PRÓXIMOS PASOS:${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo "OPCIÓN 1: Crear y probar cluster (Automático)"
echo -e "  ${GREEN}./start_cluster_vbox.sh${NC}"
echo ""
echo "OPCIÓN 2: Crear VM individual"
echo -e "  ${GREEN}./create_vm_vbox.sh 1${NC}"
echo -e "  ${GREEN}VBoxManage startvm DOS_Node_1 --type gui${NC}"
echo ""
echo "OPCIÓN 3: Manual completo"
echo -e "  ${GREEN}less GUIA_VIRTUALBOX.md${NC}"
echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo -e "${GREEN}✨ Sistema listo para demostración en VirtualBox${NC}"
echo -e "${GREEN}✨ Red Ad hoc funcional${NC}"
echo -e "${GREEN}✨ Descubrimiento automático de nodos${NC}"
echo ""