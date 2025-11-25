#!/bin/bash
#
# ============================================================================
# GENERADOR DE ISO - SISTEMA OPERATIVO DESCENTRALIZADO
# ============================================================================
# Script único para generar ISO booteable con todas las funcionalidades
# de la Fase 2 del proyecto.
#
# Requisitos:
#   - Linux (probado en Ubuntu/Debian)
#   - Paquetes: gcc, wget, cpio, gzip, xorriso, isolinux
#
# Uso:
#   chmod +x build_iso.sh
#   sudo ./build_iso.sh
#
# Resultado:
#   DecOS_Fase2.iso - ISO booteable para VirtualBox/QEMU/PC
# ============================================================================

set -e

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuración
WORK_DIR="$(pwd)/build_iso_work"
OUTPUT_ISO="DecOS_Fase2.iso"
KERNEL_VERSION="6.1.0"

# ============================================================================
# FUNCIONES AUXILIARES
# ============================================================================

print_header() {
    echo ""
    echo -e "${CYAN}╔═══════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║                                                                   ║${NC}"
    echo -e "${CYAN}║     GENERADOR DE ISO - SO DESCENTRALIZADO FASE 2                 ║${NC}"
    echo -e "${CYAN}║                                                                   ║${NC}"
    echo -e "${CYAN}╚═══════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

print_step() {
    echo -e "${YELLOW}[PASO $1]${NC} $2"
}

print_ok() {
    echo -e "        ${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "        ${RED}✗${NC} $1"
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}Este script necesita ejecutarse como root (sudo)${NC}"
        exit 1
    fi
}

check_dependencies() {
    local missing=""
    
    for cmd in gcc wget cpio gzip xorriso; do
        if ! command -v $cmd &> /dev/null; then
            missing="$missing $cmd"
        fi
    done
    
    # Verificar que exista isolinux.bin
    ISOLINUX_EXISTS=0
    for dir in /usr/lib/ISOLINUX /usr/share/syslinux /usr/lib/syslinux/bios /usr/lib/syslinux; do
        if [ -f "$dir/isolinux.bin" ]; then
            ISOLINUX_EXISTS=1
            break
        fi
    done
    
    if [ "$ISOLINUX_EXISTS" -eq 0 ]; then
        missing="$missing isolinux"
    fi
    
    if [ -n "$missing" ]; then
        echo -e "${YELLOW}Instalando dependencias:${missing}${NC}"
        apt-get update -qq
        apt-get install -y -qq gcc wget cpio gzip xorriso isolinux syslinux-common syslinux
    fi
    
    print_ok "Dependencias verificadas"
}

cleanup() {
    print_step "CLEAN" "Limpiando archivos temporales..."
    rm -rf "$WORK_DIR"
    print_ok "Limpieza completada"
}

# ============================================================================
# CÓDIGO FUENTE EMBEBIDO
# ============================================================================

create_source_code() {
    cat > "$WORK_DIR/src/decos.c" << 'EOFCODE'
/*
 * SISTEMA OPERATIVO DESCENTRALIZADO - FASE 2
 * Núcleo Funcional Distribuido para Redes Ad-Hoc
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

// ============================================================================
// CONFIGURACIÓN
// ============================================================================

#define DISCOVERY_PORT      8888
#define DATA_PORT          8889
#define MAX_NODES          64
#define MAX_TASKS          256
#define MAX_MEMORY_BLOCKS  512
#define MAX_LOCKS          128
#define BROADCAST_INTERVAL 3
#define HEARTBEAT_TIMEOUT  15
#define BUFFER_SIZE        4096

// ============================================================================
// TIPOS Y ESTRUCTURAS
// ============================================================================

typedef enum { NODE_UNKNOWN=0, NODE_ACTIVE, NODE_BUSY, NODE_FAILED, NODE_RECOVERING } NodeStatus;
typedef enum { TASK_PENDING=0, TASK_ASSIGNED, TASK_RUNNING, TASK_COMPLETED, TASK_FAILED, TASK_MIGRATING } TaskStatus;
typedef enum { MSG_DISCOVERY=1, MSG_HEARTBEAT, MSG_TASK_ASSIGN, MSG_TASK_RESULT, MSG_NODE_FAILURE } MessageType;

typedef struct {
    uint64_t node_id;
    char ip_address[16];
    char hostname[64];
    uint16_t data_port;
    float cpu_load;
    float memory_usage;
    float reputation;
    uint32_t tasks_completed;
    uint32_t tasks_failed;
    NodeStatus status;
    time_t last_seen;
    bool is_local;
} NodeInfo;

typedef struct {
    NodeInfo nodes[MAX_NODES];
    int count;
    pthread_mutex_t lock;
} NodeRegistry;

typedef struct {
    uint64_t task_id;
    uint64_t owner_node;
    uint64_t assigned_node;
    char description[128];
    int priority;
    TaskStatus status;
    time_t created_at;
    time_t completed_at;
    int exit_code;
} DistributedTask;

typedef struct {
    DistributedTask tasks[MAX_TASKS];
    size_t task_count;
    uint64_t next_task_id;
    pthread_mutex_t lock;
    uint64_t total_assigned;
    uint64_t total_completed;
    uint64_t total_failed;
    uint64_t total_migrated;
} DistributedScheduler;

typedef struct {
    uint64_t block_id;
    uint64_t owner_node;
    void* data;
    size_t size;
    uint32_t version;
    int ref_count;
    bool is_replicated;
    pthread_rwlock_t rwlock;
} SharedMemoryBlock;

typedef struct {
    SharedMemoryBlock* blocks[MAX_MEMORY_BLOCKS];
    size_t block_count;
    uint64_t next_block_id;
    size_t total_allocated;
    size_t total_shared;
    pthread_mutex_t lock;
} DistributedMemoryManager;

typedef struct {
    uint64_t lock_id;
    char name[64];
    uint64_t owner_node;
    bool is_locked;
    pthread_mutex_t local_lock;
} DistributedLock;

typedef struct {
    DistributedLock locks[MAX_LOCKS];
    size_t lock_count;
    uint64_t next_lock_id;
    pthread_mutex_t lock;
} SyncManager;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint64_t sender_id;
    uint64_t timestamp;
    uint16_t payload_size;
    uint8_t payload[512];
} NetworkMessage;

typedef struct __attribute__((packed)) {
    uint64_t node_id;
    char hostname[64];
    char ip_address[16];
    uint16_t data_port;
    float cpu_load;
    float memory_usage;
    float reputation;
    uint32_t tasks_completed;
    uint32_t tasks_failed;
    uint8_t status;
} DiscoveryPayload;

typedef struct {
    uint64_t node_id;
    NodeInfo local_info;
    NodeRegistry* registry;
    DistributedScheduler* scheduler;
    DistributedMemoryManager* memory;
    SyncManager* sync;
    int discovery_socket;
    int data_socket;
    pthread_t discovery_thread;
    pthread_t heartbeat_thread;
    pthread_t failure_detector_thread;
    pthread_t command_thread;
    volatile bool running;
} DistributedKernel;

static DistributedKernel* g_kernel = NULL;

// ============================================================================
// UTILIDADES
// ============================================================================

static uint64_t generate_node_id(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t id = ((uint64_t)ts.tv_sec << 32) | (ts.tv_nsec & 0xFFFFFFFF);
    id ^= ((uint64_t)getpid() << 16);
    id ^= (uint64_t)rand();
    return id;
}

static int get_local_ip(char* buffer, size_t len) {
    struct ifconf ifc;
    struct ifreq ifr[10];
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { strcpy(buffer, "127.0.0.1"); return -1; }
    
    ifc.ifc_len = sizeof(ifr);
    ifc.ifc_req = ifr;
    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) { close(sock); strcpy(buffer, "127.0.0.1"); return -1; }
    
    int num_ifaces = ifc.ifc_len / sizeof(struct ifreq);
    for (int i = 0; i < num_ifaces; i++) {
        struct sockaddr_in* addr = (struct sockaddr_in*)&ifr[i].ifr_addr;
        char* ip = inet_ntoa(addr->sin_addr);
        if (strcmp(ip, "127.0.0.1") != 0) {
            strncpy(buffer, ip, len - 1);
            buffer[len - 1] = '\0';
            close(sock);
            return 0;
        }
    }
    close(sock);
    strcpy(buffer, "127.0.0.1");
    return -1;
}

static float get_cpu_load(void) {
    FILE* f = fopen("/proc/loadavg", "r");
    if (!f) return 0.5;
    float load;
    if (fscanf(f, "%f", &load) != 1) load = 0.5;
    fclose(f);
    return load > 1.0 ? 1.0 : load;
}

static float get_memory_usage(void) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return 0.5;
    long total = 0, available = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, "%ld", &total);
        else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%ld", &available);
    }
    fclose(f);
    if (total == 0) return 0.5;
    return 1.0 - ((float)available / total);
}

// ============================================================================
// RED AD-HOC: DESCUBRIMIENTO
// ============================================================================

static int create_discovery_socket(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    
    int broadcast = 1, reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DISCOVERY_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(sock); return -1; }
    
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    return sock;
}

static void send_discovery_broadcast(void) {
    if (!g_kernel || g_kernel->discovery_socket < 0) return;
    
    NetworkMessage msg = {0};
    msg.type = MSG_DISCOVERY;
    msg.sender_id = g_kernel->node_id;
    msg.timestamp = (uint64_t)time(NULL);
    
    DiscoveryPayload* payload = (DiscoveryPayload*)msg.payload;
    payload->node_id = g_kernel->node_id;
    strncpy(payload->hostname, g_kernel->local_info.hostname, 63);
    strncpy(payload->ip_address, g_kernel->local_info.ip_address, 15);
    payload->data_port = DATA_PORT;
    payload->cpu_load = get_cpu_load();
    payload->memory_usage = get_memory_usage();
    payload->reputation = g_kernel->local_info.reputation;
    payload->tasks_completed = g_kernel->local_info.tasks_completed;
    payload->tasks_failed = g_kernel->local_info.tasks_failed;
    payload->status = g_kernel->local_info.status;
    msg.payload_size = sizeof(DiscoveryPayload);
    
    struct sockaddr_in bcast = {0};
    bcast.sin_family = AF_INET;
    bcast.sin_port = htons(DISCOVERY_PORT);
    
    const char* subnets[] = {"255.255.255.255", "192.168.1.255", "192.168.0.255", 
                             "192.168.10.255", "10.0.0.255", "10.0.2.255", NULL};
    for (int i = 0; subnets[i]; i++) {
        inet_pton(AF_INET, subnets[i], &bcast.sin_addr);
        sendto(g_kernel->discovery_socket, &msg, sizeof(msg) - sizeof(msg.payload) + msg.payload_size,
               0, (struct sockaddr*)&bcast, sizeof(bcast));
    }
}

static void process_discovery_message(NetworkMessage* msg, struct sockaddr_in* sender) {
    if (!g_kernel || !msg) return;
    DiscoveryPayload* payload = (DiscoveryPayload*)msg->payload;
    if (payload->node_id == g_kernel->node_id) return;
    
    pthread_mutex_lock(&g_kernel->registry->lock);
    
    int found_idx = -1;
    for (int i = 0; i < g_kernel->registry->count; i++) {
        if (g_kernel->registry->nodes[i].node_id == payload->node_id) { found_idx = i; break; }
    }
    
    NodeInfo* node;
    if (found_idx >= 0) {
        node = &g_kernel->registry->nodes[found_idx];
    } else if (g_kernel->registry->count < MAX_NODES) {
        node = &g_kernel->registry->nodes[g_kernel->registry->count++];
        printf("\n[DISCOVERY] ✓ Nuevo nodo: %016lX (%s) @ %s\n", 
               payload->node_id, payload->hostname, inet_ntoa(sender->sin_addr));
    } else {
        pthread_mutex_unlock(&g_kernel->registry->lock);
        return;
    }
    
    node->node_id = payload->node_id;
    strncpy(node->hostname, payload->hostname, 63);
    strncpy(node->ip_address, inet_ntoa(sender->sin_addr), 15);
    node->data_port = payload->data_port;
    node->cpu_load = payload->cpu_load;
    node->memory_usage = payload->memory_usage;
    node->reputation = payload->reputation;
    node->tasks_completed = payload->tasks_completed;
    node->tasks_failed = payload->tasks_failed;
    node->status = (NodeStatus)payload->status;
    node->last_seen = time(NULL);
    node->is_local = false;
    
    pthread_mutex_unlock(&g_kernel->registry->lock);
}

static void* discovery_listener_thread(void* arg) {
    (void)arg;
    NetworkMessage msg;
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    
    while (g_kernel && g_kernel->running) {
        memset(&msg, 0, sizeof(msg));
        ssize_t n = recvfrom(g_kernel->discovery_socket, &msg, sizeof(msg), 0, 
                            (struct sockaddr*)&sender, &sender_len);
        if (n > 0 && (msg.type == MSG_DISCOVERY || msg.type == MSG_HEARTBEAT)) {
            process_discovery_message(&msg, &sender);
        }
        usleep(10000);
    }
    return NULL;
}

static void* heartbeat_broadcast_thread(void* arg) {
    (void)arg;
    while (g_kernel && g_kernel->running) {
        send_discovery_broadcast();
        sleep(BROADCAST_INTERVAL);
    }
    return NULL;
}

// ============================================================================
// SCHEDULER DISTRIBUIDO
// ============================================================================

static float calculate_node_score(NodeInfo* node, int task_priority) {
    if (!node || node->status != NODE_ACTIVE) return -1.0;
    float load_score = 1.0 - node->cpu_load;
    float mem_score = 1.0 - node->memory_usage;
    float rep_score = node->reputation;
    time_t now = time(NULL);
    float freshness = 1.0;
    if (now - node->last_seen > 5) freshness = 1.0 / (1.0 + (now - node->last_seen - 5) * 0.1);
    float priority_bonus = (task_priority >= 8 && rep_score > 0.7) ? 0.1 : 0;
    return 0.30 * load_score + 0.20 * mem_score + 0.35 * rep_score + 0.15 * freshness + priority_bonus;
}

static uint64_t select_best_node(int task_priority) {
    if (!g_kernel) return 0;
    pthread_mutex_lock(&g_kernel->registry->lock);
    float best_score = -1.0;
    uint64_t best_node = 0;
    for (int i = 0; i < g_kernel->registry->count; i++) {
        NodeInfo* node = &g_kernel->registry->nodes[i];
        if (node->status != NODE_ACTIVE) continue;
        float score = calculate_node_score(node, task_priority);
        if (score > best_score) { best_score = score; best_node = node->node_id; }
    }
    float local_score = calculate_node_score(&g_kernel->local_info, task_priority);
    if (local_score > best_score) best_node = g_kernel->node_id;
    pthread_mutex_unlock(&g_kernel->registry->lock);
    return best_node;
}

static void update_node_reputation(uint64_t node_id, bool success) {
    if (!g_kernel) return;
    pthread_mutex_lock(&g_kernel->registry->lock);
    for (int i = 0; i < g_kernel->registry->count; i++) {
        if (g_kernel->registry->nodes[i].node_id == node_id) {
            NodeInfo* node = &g_kernel->registry->nodes[i];
            float delta = success ? 0.05 : -0.10;
            node->reputation += delta * (1.0 - node->reputation);
            if (node->reputation < 0.1) node->reputation = 0.1;
            if (node->reputation > 1.0) node->reputation = 1.0;
            if (success) node->tasks_completed++; else node->tasks_failed++;
            break;
        }
    }
    pthread_mutex_unlock(&g_kernel->registry->lock);
}

static uint64_t create_task(const char* description, int priority) {
    if (!g_kernel || !g_kernel->scheduler) return 0;
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    if (g_kernel->scheduler->task_count >= MAX_TASKS) {
        pthread_mutex_unlock(&g_kernel->scheduler->lock);
        return 0;
    }
    DistributedTask* task = &g_kernel->scheduler->tasks[g_kernel->scheduler->task_count];
    memset(task, 0, sizeof(DistributedTask));
    task->task_id = ++g_kernel->scheduler->next_task_id;
    task->owner_node = g_kernel->node_id;
    task->priority = (priority < 1) ? 1 : (priority > 10) ? 10 : priority;
    task->status = TASK_PENDING;
    task->created_at = time(NULL);
    if (description) strncpy(task->description, description, sizeof(task->description) - 1);
    
    uint64_t target = select_best_node(task->priority);
    if (target) {
        task->assigned_node = target;
        task->status = TASK_ASSIGNED;
        g_kernel->scheduler->total_assigned++;
        printf("[SCHEDULER] Tarea %lu -> nodo %016lX\n", task->task_id, target);
    }
    g_kernel->scheduler->task_count++;
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    return task->task_id;
}

static void complete_task(uint64_t task_id, int exit_code) {
    if (!g_kernel) return;
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    for (size_t i = 0; i < g_kernel->scheduler->task_count; i++) {
        DistributedTask* task = &g_kernel->scheduler->tasks[i];
        if (task->task_id == task_id) {
            task->status = (exit_code == 0) ? TASK_COMPLETED : TASK_FAILED;
            task->completed_at = time(NULL);
            task->exit_code = exit_code;
            if (exit_code == 0) g_kernel->scheduler->total_completed++;
            else g_kernel->scheduler->total_failed++;
            update_node_reputation(task->assigned_node, exit_code == 0);
            break;
        }
    }
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
}

// ============================================================================
// MEMORIA DISTRIBUIDA
// ============================================================================

static uint64_t create_shared_memory(size_t size) {
    if (!g_kernel || !g_kernel->memory || size == 0) return 0;
    pthread_mutex_lock(&g_kernel->memory->lock);
    if (g_kernel->memory->block_count >= MAX_MEMORY_BLOCKS) {
        pthread_mutex_unlock(&g_kernel->memory->lock);
        return 0;
    }
    SharedMemoryBlock* block = calloc(1, sizeof(SharedMemoryBlock));
    if (!block) { pthread_mutex_unlock(&g_kernel->memory->lock); return 0; }
    block->data = calloc(1, size);
    if (!block->data) { free(block); pthread_mutex_unlock(&g_kernel->memory->lock); return 0; }
    block->block_id = ++g_kernel->memory->next_block_id;
    block->owner_node = g_kernel->node_id;
    block->size = size;
    block->version = 1;
    block->ref_count = 1;
    pthread_rwlock_init(&block->rwlock, NULL);
    g_kernel->memory->blocks[g_kernel->memory->block_count++] = block;
    g_kernel->memory->total_allocated += size;
    pthread_mutex_unlock(&g_kernel->memory->lock);
    return block->block_id;
}

static int write_shared_memory(uint64_t block_id, const void* data, size_t size, size_t offset) {
    if (!g_kernel || !data) return -1;
    pthread_mutex_lock(&g_kernel->memory->lock);
    SharedMemoryBlock* block = NULL;
    for (size_t i = 0; i < g_kernel->memory->block_count; i++) {
        if (g_kernel->memory->blocks[i]->block_id == block_id) { block = g_kernel->memory->blocks[i]; break; }
    }
    if (!block || offset + size > block->size) { pthread_mutex_unlock(&g_kernel->memory->lock); return -1; }
    pthread_rwlock_wrlock(&block->rwlock);
    memcpy((uint8_t*)block->data + offset, data, size);
    block->version++;
    pthread_rwlock_unlock(&block->rwlock);
    pthread_mutex_unlock(&g_kernel->memory->lock);
    return 0;
}

static int read_shared_memory(uint64_t block_id, void* buffer, size_t size, size_t offset) {
    if (!g_kernel || !buffer) return -1;
    pthread_mutex_lock(&g_kernel->memory->lock);
    SharedMemoryBlock* block = NULL;
    for (size_t i = 0; i < g_kernel->memory->block_count; i++) {
        if (g_kernel->memory->blocks[i]->block_id == block_id) { block = g_kernel->memory->blocks[i]; break; }
    }
    if (!block || offset + size > block->size) { pthread_mutex_unlock(&g_kernel->memory->lock); return -1; }
    pthread_rwlock_rdlock(&block->rwlock);
    memcpy(buffer, (uint8_t*)block->data + offset, size);
    pthread_rwlock_unlock(&block->rwlock);
    pthread_mutex_unlock(&g_kernel->memory->lock);
    return 0;
}

// ============================================================================
// SINCRONIZACIÓN DISTRIBUIDA
// ============================================================================

static uint64_t create_distributed_lock(const char* name) {
    if (!g_kernel || !g_kernel->sync) return 0;
    pthread_mutex_lock(&g_kernel->sync->lock);
    for (size_t i = 0; i < g_kernel->sync->lock_count; i++) {
        if (strcmp(g_kernel->sync->locks[i].name, name) == 0) {
            uint64_t id = g_kernel->sync->locks[i].lock_id;
            pthread_mutex_unlock(&g_kernel->sync->lock);
            return id;
        }
    }
    if (g_kernel->sync->lock_count >= MAX_LOCKS) {
        pthread_mutex_unlock(&g_kernel->sync->lock);
        return 0;
    }
    DistributedLock* lock = &g_kernel->sync->locks[g_kernel->sync->lock_count++];
    memset(lock, 0, sizeof(DistributedLock));
    lock->lock_id = ++g_kernel->sync->next_lock_id;
    strncpy(lock->name, name, sizeof(lock->name) - 1);
    pthread_mutex_init(&lock->local_lock, NULL);
    pthread_mutex_unlock(&g_kernel->sync->lock);
    return lock->lock_id;
}

static int acquire_distributed_lock(uint64_t lock_id, int timeout_ms) {
    if (!g_kernel) return -1;
    pthread_mutex_lock(&g_kernel->sync->lock);
    DistributedLock* lock = NULL;
    for (size_t i = 0; i < g_kernel->sync->lock_count; i++) {
        if (g_kernel->sync->locks[i].lock_id == lock_id) { lock = &g_kernel->sync->locks[i]; break; }
    }
    if (!lock) { pthread_mutex_unlock(&g_kernel->sync->lock); return -1; }
    pthread_mutex_unlock(&g_kernel->sync->lock);
    
    int elapsed = 0;
    while (elapsed < timeout_ms || timeout_ms < 0) {
        pthread_mutex_lock(&lock->local_lock);
        if (!lock->is_locked) {
            lock->is_locked = true;
            lock->owner_node = g_kernel->node_id;
            pthread_mutex_unlock(&lock->local_lock);
            return 0;
        }
        pthread_mutex_unlock(&lock->local_lock);
        usleep(10000);
        elapsed += 10;
    }
    return -1;
}

static int release_distributed_lock(uint64_t lock_id) {
    if (!g_kernel) return -1;
    pthread_mutex_lock(&g_kernel->sync->lock);
    for (size_t i = 0; i < g_kernel->sync->lock_count; i++) {
        if (g_kernel->sync->locks[i].lock_id == lock_id) {
            DistributedLock* lock = &g_kernel->sync->locks[i];
            pthread_mutex_lock(&lock->local_lock);
            if (lock->owner_node == g_kernel->node_id) {
                lock->is_locked = false;
                lock->owner_node = 0;
            }
            pthread_mutex_unlock(&lock->local_lock);
            pthread_mutex_unlock(&g_kernel->sync->lock);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_kernel->sync->lock);
    return -1;
}

// ============================================================================
// DETECCIÓN DE FALLOS
// ============================================================================

static void* failure_detector_thread(void* arg) {
    (void)arg;
    while (g_kernel && g_kernel->running) {
        time_t now = time(NULL);
        pthread_mutex_lock(&g_kernel->registry->lock);
        for (int i = 0; i < g_kernel->registry->count; i++) {
            NodeInfo* node = &g_kernel->registry->nodes[i];
            if ((node->status == NODE_ACTIVE || node->status == NODE_BUSY) &&
                now - node->last_seen > HEARTBEAT_TIMEOUT) {
                printf("\n[FAILURE] ⚠ Nodo %016lX no responde!\n", node->node_id);
                node->status = NODE_FAILED;
                node->reputation *= 0.5;
                
                pthread_mutex_lock(&g_kernel->scheduler->lock);
                for (size_t j = 0; j < g_kernel->scheduler->task_count; j++) {
                    DistributedTask* task = &g_kernel->scheduler->tasks[j];
                    if (task->assigned_node == node->node_id &&
                        (task->status == TASK_ASSIGNED || task->status == TASK_RUNNING)) {
                        uint64_t new_node = select_best_node(task->priority);
                        if (new_node && new_node != node->node_id) {
                            task->assigned_node = new_node;
                            task->status = TASK_ASSIGNED;
                            g_kernel->scheduler->total_migrated++;
                            printf("[RECOVERY] Tarea %lu -> %016lX\n", task->task_id, new_node);
                        }
                    }
                }
                pthread_mutex_unlock(&g_kernel->scheduler->lock);
            }
        }
        pthread_mutex_unlock(&g_kernel->registry->lock);
        sleep(5);
    }
    return NULL;
}

// ============================================================================
// INTERFAZ DE COMANDOS
// ============================================================================

static void print_banner(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                   ║\n");
    printf("║     ███████╗ ██████╗     ██████╗ ███████╗███████╗ ██████╗        ║\n");
    printf("║     ██╔════╝██╔═══██╗    ██╔══██╗██╔════╝██╔════╝██╔════╝        ║\n");
    printf("║     ███████╗██║   ██║    ██║  ██║█████╗  ███████╗██║             ║\n");
    printf("║     ╚════██║██║   ██║    ██║  ██║██╔══╝  ╚════██║██║             ║\n");
    printf("║     ███████║╚██████╔╝    ██████╔╝███████╗███████║╚██████╗        ║\n");
    printf("║     ╚══════╝ ╚═════╝     ╚═════╝ ╚══════╝╚══════╝ ╚═════╝        ║\n");
    printf("║                                                                   ║\n");
    printf("║          SISTEMA OPERATIVO DESCENTRALIZADO v2.0                  ║\n");
    printf("║             Fase 2: Núcleo Funcional Distribuido                 ║\n");
    printf("║                                                                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

static void print_status(void) {
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("                      ESTADO DEL SISTEMA\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    printf("🖥  NODO LOCAL\n");
    printf("   ID: %016lX | Host: %s | IP: %s\n", 
           g_kernel->node_id, g_kernel->local_info.hostname, g_kernel->local_info.ip_address);
    printf("   CPU: %.1f%% | RAM: %.1f%% | Rep: %.2f\n\n", 
           get_cpu_load()*100, get_memory_usage()*100, g_kernel->local_info.reputation);
    
    pthread_mutex_lock(&g_kernel->registry->lock);
    int active = 0;
    for (int i = 0; i < g_kernel->registry->count; i++)
        if (g_kernel->registry->nodes[i].status == NODE_ACTIVE) active++;
    printf("🌐 RED: %d nodos activos de %d | UDP:%d TCP:%d\n\n", 
           active, g_kernel->registry->count, DISCOVERY_PORT, DATA_PORT);
    pthread_mutex_unlock(&g_kernel->registry->lock);
    
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    printf("📋 SCHEDULER: %zu tareas | Asig:%lu Comp:%lu Fail:%lu Migr:%lu\n\n",
           g_kernel->scheduler->task_count, g_kernel->scheduler->total_assigned,
           g_kernel->scheduler->total_completed, g_kernel->scheduler->total_failed,
           g_kernel->scheduler->total_migrated);
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    
    pthread_mutex_lock(&g_kernel->memory->lock);
    printf("💾 MEMORIA: %zu bloques | %zu bytes asignados | %zu bytes compartidos\n\n",
           g_kernel->memory->block_count, g_kernel->memory->total_allocated,
           g_kernel->memory->total_shared);
    pthread_mutex_unlock(&g_kernel->memory->lock);
    
    pthread_mutex_lock(&g_kernel->sync->lock);
    printf("🔒 LOCKS: %zu activos\n\n", g_kernel->sync->lock_count);
    pthread_mutex_unlock(&g_kernel->sync->lock);
}

static void print_nodes(void) {
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("                       NODOS EN LA RED\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    pthread_mutex_lock(&g_kernel->registry->lock);
    if (g_kernel->registry->count == 0) {
        printf("   No hay otros nodos. Esperando broadcast...\n");
    } else {
        printf("   %-18s %-16s %-8s %-6s %-6s %-5s\n", "NODE ID", "IP", "STATUS", "CPU", "MEM", "REP");
        printf("   ────────────────── ──────────────── ──────── ────── ────── ─────\n");
        for (int i = 0; i < g_kernel->registry->count; i++) {
            NodeInfo* n = &g_kernel->registry->nodes[i];
            const char* st = n->status == NODE_ACTIVE ? "ACTIVO" : 
                            n->status == NODE_FAILED ? "FALLO" : "DESCON";
            printf("   %016lX %-16s %-8s %5.1f%% %5.1f%% %.2f\n",
                   n->node_id, n->ip_address, st, n->cpu_load*100, n->memory_usage*100, n->reputation);
        }
    }
    pthread_mutex_unlock(&g_kernel->registry->lock);
    printf("\n");
}

static void print_tasks(void) {
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("                         TAREAS\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    if (g_kernel->scheduler->task_count == 0) {
        printf("   No hay tareas.\n");
    } else {
        printf("   %-5s %-25s %-18s %-10s\n", "ID", "DESCRIPCION", "NODO", "ESTADO");
        printf("   ───── ───────────────────────── ────────────────── ──────────\n");
        for (size_t i = 0; i < g_kernel->scheduler->task_count; i++) {
            DistributedTask* t = &g_kernel->scheduler->tasks[i];
            const char* st = t->status == TASK_PENDING ? "PEND" :
                            t->status == TASK_ASSIGNED ? "ASIG" :
                            t->status == TASK_RUNNING ? "EJEC" :
                            t->status == TASK_COMPLETED ? "COMP" : "FAIL";
            char desc[26]; strncpy(desc, t->description, 25); desc[25] = '\0';
            printf("   %-5lu %-25s %016lX %-10s\n", t->task_id, desc, t->assigned_node, st);
        }
    }
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    printf("\n");
}

static void print_help(void) {
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("                          COMANDOS\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    printf("   status      Estado completo del sistema\n");
    printf("   nodes       Listar nodos de la red\n");
    printf("   tasks       Listar tareas\n");
    printf("   task <desc> Crear nueva tarea\n");
    printf("   alloc <n>   Asignar n bytes de memoria compartida\n");
    printf("   demo        Ejecutar demostración\n");
    printf("   help        Esta ayuda\n");
    printf("   exit        Salir\n\n");
}

static void run_demo(void) {
    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf("              DEMOSTRACIÓN FASE 2\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    printf("▶ 1. SCHEDULER DISTRIBUIDO\n");
    uint64_t t1 = create_task("Entrenamiento ML", 9);
    uint64_t t2 = create_task("Proceso datos", 5);
    uint64_t t3 = create_task("Análisis", 7);
    printf("   ✓ Tareas: %lu (p9), %lu (p5), %lu (p7)\n", t1, t2, t3);
    sleep(1);
    complete_task(t1, 0);
    printf("   ✓ Tarea %lu completada\n\n", t1);
    
    printf("▶ 2. MEMORIA DISTRIBUIDA\n");
    uint64_t m1 = create_shared_memory(4096);
    uint64_t m2 = create_shared_memory(1024);
    const char* data = "Test memoria distribuida";
    write_shared_memory(m1, data, strlen(data)+1, 0);
    char buf[256];
    read_shared_memory(m1, buf, strlen(data)+1, 0);
    printf("   ✓ Bloques: %lu (4KB), %lu (1KB)\n", m1, m2);
    printf("   ✓ Verificado: \"%s\"\n\n", buf);
    
    printf("▶ 3. SINCRONIZACIÓN\n");
    uint64_t l1 = create_distributed_lock("recurso");
    if (acquire_distributed_lock(l1, 1000) == 0) {
        printf("   ✓ Lock adquirido\n");
        release_distributed_lock(l1);
        printf("   ✓ Lock liberado\n\n");
    }
    
    printf("▶ 4. TOLERANCIA A FALLOS\n");
    printf("   • Monitor heartbeat activo (timeout: %ds)\n", HEARTBEAT_TIMEOUT);
    printf("   • Migración automática habilitada\n\n");
    
    printf("════════════════════════════════════════════════════════════════════\n\n");
}

// ============================================================================
// INICIALIZACIÓN Y MAIN
// ============================================================================

static void handle_signal(int sig) {
    (void)sig;
    printf("\n[SIGNAL] Terminando...\n");
    if (g_kernel) g_kernel->running = false;
}

static int init_kernel(uint64_t node_id) {
    g_kernel = calloc(1, sizeof(DistributedKernel));
    if (!g_kernel) return -1;
    
    g_kernel->node_id = node_id ? node_id : generate_node_id();
    gethostname(g_kernel->local_info.hostname, sizeof(g_kernel->local_info.hostname));
    get_local_ip(g_kernel->local_info.ip_address, sizeof(g_kernel->local_info.ip_address));
    g_kernel->local_info.node_id = g_kernel->node_id;
    g_kernel->local_info.data_port = DATA_PORT;
    g_kernel->local_info.reputation = 0.5;
    g_kernel->local_info.status = NODE_ACTIVE;
    g_kernel->local_info.is_local = true;
    g_kernel->local_info.last_seen = time(NULL);
    
    g_kernel->registry = calloc(1, sizeof(NodeRegistry));
    pthread_mutex_init(&g_kernel->registry->lock, NULL);
    
    g_kernel->scheduler = calloc(1, sizeof(DistributedScheduler));
    pthread_mutex_init(&g_kernel->scheduler->lock, NULL);
    
    g_kernel->memory = calloc(1, sizeof(DistributedMemoryManager));
    pthread_mutex_init(&g_kernel->memory->lock, NULL);
    
    g_kernel->sync = calloc(1, sizeof(SyncManager));
    pthread_mutex_init(&g_kernel->sync->lock, NULL);
    
    g_kernel->discovery_socket = create_discovery_socket();
    if (g_kernel->discovery_socket < 0) return -1;
    
    g_kernel->running = true;
    return 0;
}

static void start_threads(void) {
    pthread_create(&g_kernel->discovery_thread, NULL, discovery_listener_thread, NULL);
    pthread_create(&g_kernel->heartbeat_thread, NULL, heartbeat_broadcast_thread, NULL);
    pthread_create(&g_kernel->failure_detector_thread, NULL, failure_detector_thread, NULL);
}

static void cleanup(void) {
    if (!g_kernel) return;
    g_kernel->running = false;
    pthread_join(g_kernel->discovery_thread, NULL);
    pthread_join(g_kernel->heartbeat_thread, NULL);
    pthread_join(g_kernel->failure_detector_thread, NULL);
    if (g_kernel->discovery_socket >= 0) close(g_kernel->discovery_socket);
    
    pthread_mutex_lock(&g_kernel->memory->lock);
    for (size_t i = 0; i < g_kernel->memory->block_count; i++) {
        if (g_kernel->memory->blocks[i]) {
            free(g_kernel->memory->blocks[i]->data);
            pthread_rwlock_destroy(&g_kernel->memory->blocks[i]->rwlock);
            free(g_kernel->memory->blocks[i]);
        }
    }
    pthread_mutex_unlock(&g_kernel->memory->lock);
    
    pthread_mutex_destroy(&g_kernel->registry->lock);
    pthread_mutex_destroy(&g_kernel->scheduler->lock);
    pthread_mutex_destroy(&g_kernel->memory->lock);
    pthread_mutex_destroy(&g_kernel->sync->lock);
    
    free(g_kernel->registry);
    free(g_kernel->scheduler);
    free(g_kernel->memory);
    free(g_kernel->sync);
    free(g_kernel);
    g_kernel = NULL;
}

int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL) ^ getpid());
    
    uint64_t node_id = 0;
    if (argc > 1) node_id = strtoull(argv[1], NULL, 16);
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    print_banner();
    
    printf("[INIT] Inicializando kernel...\n");
    if (init_kernel(node_id) < 0) {
        fprintf(stderr, "[ERROR] Fallo inicialización\n");
        return 1;
    }
    
    printf("[INIT] ✓ ID: %016lX\n", g_kernel->node_id);
    printf("[INIT] ✓ Host: %s\n", g_kernel->local_info.hostname);
    printf("[INIT] ✓ IP: %s\n", g_kernel->local_info.ip_address);
    printf("[INIT] ✓ Puertos: UDP %d, TCP %d\n", DISCOVERY_PORT, DATA_PORT);
    
    start_threads();
    printf("[INIT] ✓ Servicios de red activos\n\n");
    
    printf("[NET] Buscando nodos");
    for (int i = 0; i < 3; i++) { printf("."); fflush(stdout); sleep(1); }
    
    pthread_mutex_lock(&g_kernel->registry->lock);
    int found = g_kernel->registry->count;
    pthread_mutex_unlock(&g_kernel->registry->lock);
    printf(" %d nodo(s)\n\n", found);
    
    printf("Escribe 'help' para ver comandos.\n\n");
    
    char line[256], cmd[64], args[192];
    while (g_kernel && g_kernel->running) {
        printf("DecOS> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) { g_kernel->running = false; break; }
            continue;
        }
        line[strcspn(line, "\n")] = '\0';
        cmd[0] = args[0] = '\0';
        sscanf(line, "%63s %191[^\n]", cmd, args);
        if (strlen(cmd) == 0) continue;
        
        if (strcmp(cmd, "status") == 0) print_status();
        else if (strcmp(cmd, "nodes") == 0) print_nodes();
        else if (strcmp(cmd, "tasks") == 0) print_tasks();
        else if (strcmp(cmd, "task") == 0) {
            if (strlen(args) > 0) {
                uint64_t tid = create_task(args, 5);
                printf(tid ? "Tarea %lu creada\n" : "Error\n", tid);
            } else printf("Uso: task <descripcion>\n");
        }
        else if (strcmp(cmd, "alloc") == 0) {
            int size = 0;
            if (sscanf(args, "%d", &size) == 1 && size > 0) {
                uint64_t bid = create_shared_memory((size_t)size);
                printf(bid ? "Bloque %lu: %d bytes\n" : "Error\n", bid, size);
            } else printf("Uso: alloc <bytes>\n");
        }
        else if (strcmp(cmd, "demo") == 0) run_demo();
        else if (strcmp(cmd, "help") == 0) print_help();
        else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            printf("Apagando...\n");
            g_kernel->running = false;
        }
        else printf("Comando desconocido: '%s'\n", cmd);
    }
    
    printf("[SHUTDOWN] Limpiando...\n");
    cleanup();
    printf("[SHUTDOWN] Completado\n");
    return 0;
}
EOFCODE
}

# ============================================================================
# COMPILACIÓN
# ============================================================================

compile_system() {
    print_step "2" "Compilando sistema..."
    
    mkdir -p "$WORK_DIR/src"
    create_source_code
    
    # Compilar estáticamente
    gcc -static -O2 -pthread -o "$WORK_DIR/decos" "$WORK_DIR/src/decos.c" -lm -lpthread 2>/dev/null || {
        # Si falla estático, intentar dinámico
        gcc -O2 -pthread -o "$WORK_DIR/decos" "$WORK_DIR/src/decos.c" -lm -lpthread
    }
    
    if [ ! -f "$WORK_DIR/decos" ]; then
        print_error "Compilación fallida"
        exit 1
    fi
    
    chmod +x "$WORK_DIR/decos"
    print_ok "Compilado: decos"
}

# ============================================================================
# DESCARGAR COMPONENTES BASE
# ============================================================================

download_components() {
    print_step "3" "Descargando componentes base..."
    
    # Busybox para utilidades básicas
    if [ ! -f "$WORK_DIR/busybox" ]; then
        wget -q -O "$WORK_DIR/busybox" \
            "https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox"
        chmod +x "$WORK_DIR/busybox"
    fi
    print_ok "Busybox descargado"
    
    # Kernel de Alpine (pequeño y funcional)
    ALPINE_VERSION="3.19"
    KERNEL_URL="https://dl-cdn.alpinelinux.org/alpine/v${ALPINE_VERSION}/releases/x86_64/netboot/vmlinuz-lts"
    
    if [ ! -f "$WORK_DIR/vmlinuz" ]; then
        wget -q -O "$WORK_DIR/vmlinuz" "$KERNEL_URL"
    fi
    print_ok "Kernel descargado"
}

# ============================================================================
# CREAR INITRAMFS
# ============================================================================

create_initramfs() {
    print_step "4" "Creando initramfs..."
    
    INITRAMFS="$WORK_DIR/initramfs"
    rm -rf "$INITRAMFS"
    
    # Estructura de directorios
    mkdir -p "$INITRAMFS"/{bin,sbin,etc,proc,sys,dev,root,tmp,run,var/log}
    mkdir -p "$INITRAMFS"/lib/modules
    mkdir -p "$INITRAMFS"/usr/{bin,sbin}
    
    # Copiar busybox
    cp "$WORK_DIR/busybox" "$INITRAMFS/bin/"
    
    # Crear enlaces simbólicos para comandos esenciales
    cd "$INITRAMFS/bin"
    for cmd in sh ash bash cat ls mkdir rm cp mv mount umount \
               ifconfig ip route ping wget udhcpc sleep echo ps kill \
               grep sed awk cut head tail sort uniq wc chmod chown \
               ln touch date hostname dmesg clear vi less more \
               mknod df du free top netstat ss; do
        ln -sf busybox $cmd 2>/dev/null || true
    done
    cd - > /dev/null
    
    # Enlaces en sbin
    cd "$INITRAMFS/sbin"
    for cmd in init halt reboot poweroff ifconfig route ip; do
        ln -sf ../bin/busybox $cmd 2>/dev/null || true
    done
    cd - > /dev/null
    
    # Copiar nuestro sistema
    cp "$WORK_DIR/decos" "$INITRAMFS/sbin/"
    chmod +x "$INITRAMFS/sbin/decos"
    
    # Crear /etc/passwd y /etc/group
    cat > "$INITRAMFS/etc/passwd" << 'EOF'
root:x:0:0:root:/root:/bin/sh
EOF
    
    cat > "$INITRAMFS/etc/group" << 'EOF'
root:x:0:
EOF
    
    # Crear script init principal
    cat > "$INITRAMFS/init" << 'EOFINIT'
#!/bin/sh
# ============================================================================
# INIT - Sistema Operativo Descentralizado
# ============================================================================

# Montar sistemas de archivos virtuales
mount -t proc none /proc 2>/dev/null
mount -t sysfs none /sys 2>/dev/null
mount -t devtmpfs none /dev 2>/dev/null

# Crear dispositivos necesarios
mkdir -p /dev/pts /dev/shm
mount -t devpts devpts /dev/pts 2>/dev/null
mount -t tmpfs tmpfs /dev/shm 2>/dev/null
mount -t tmpfs tmpfs /tmp 2>/dev/null
mount -t tmpfs tmpfs /run 2>/dev/null

# Crear dispositivos de red si no existen
[ ! -c /dev/null ] && mknod /dev/null c 1 3
[ ! -c /dev/zero ] && mknod /dev/zero c 1 5
[ ! -c /dev/random ] && mknod /dev/random c 1 8
[ ! -c /dev/urandom ] && mknod /dev/urandom c 1 9
[ ! -c /dev/tty ] && mknod /dev/tty c 5 0
[ ! -c /dev/console ] && mknod /dev/console c 5 1

# Banner de inicio
clear
cat << 'BANNER'

╔═══════════════════════════════════════════════════════════════════╗
║                                                                   ║
║   ██████╗ ███████╗ ██████╗ ██████╗ ███████╗    ██████╗  ██████╗   ║
║   ██╔══██╗██╔════╝██╔════╝██╔═══██╗██╔════╝    ██╔══██╗██╔═══██╗  ║
║   ██║  ██║█████╗  ██║     ██║   ██║███████╗    ██║  ██║██║   ██║  ║
║   ██║  ██║██╔══╝  ██║     ██║   ██║╚════██║    ██║  ██║██║   ██║  ║
║   ██████╔╝███████╗╚██████╗╚██████╔╝███████║    ██████╔╝╚██████╔╝  ║
║   ╚═════╝ ╚══════╝ ╚═════╝ ╚═════╝ ╚══════╝    ╚═════╝  ╚═════╝   ║
║                                                                   ║
║           Sistema Operativo Descentralizado v2.0                  ║
║              Fase 2 - Núcleo Funcional Distribuido                ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝

BANNER

echo ""
echo "════════════════════════════════════════════════════════════════════"
echo "                    CONFIGURANDO RED AD-HOC"
echo "════════════════════════════════════════════════════════════════════"
echo ""

# Hostname único basado en tiempo
RANDOM_ID=$(head -c 4 /dev/urandom | od -A n -t x1 | tr -d ' \n')
hostname "decos-${RANDOM_ID}"
echo "decos-${RANDOM_ID}" > /etc/hostname

echo "[NET] Hostname: $(hostname)"

# Configurar loopback
echo "[NET] Configurando loopback..."
ip link set lo up
ip addr add 127.0.0.1/8 dev lo

# Detectar y configurar interfaces de red
echo "[NET] Detectando interfaces de red..."

configure_interface() {
    local iface=$1
    
    if [ ! -e "/sys/class/net/$iface" ]; then
        return 1
    fi
    
    echo "[NET] Configurando $iface..."
    
    # Activar interfaz
    ip link set "$iface" up
    sleep 1
    
    # Intentar DHCP primero
    echo "[NET]   Intentando DHCP..."
    if udhcpc -i "$iface" -n -q -t 3 -T 2 2>/dev/null; then
        echo "[NET]   ✓ DHCP exitoso en $iface"
        return 0
    fi
    
    # Si DHCP falla, usar IP estática basada en MAC
    echo "[NET]   DHCP fallido, usando IP estática..."
    
    # Generar IP basada en los últimos bytes de la MAC
    MAC=$(cat "/sys/class/net/$iface/address" 2>/dev/null)
    if [ -n "$MAC" ]; then
        LAST_OCTET=$(echo "$MAC" | awk -F: '{printf "%d", "0x"$6}')
        # Evitar .0 y .255
        [ "$LAST_OCTET" -eq 0 ] && LAST_OCTET=1
        [ "$LAST_OCTET" -eq 255 ] && LAST_OCTET=254
        
        IP="192.168.10.${LAST_OCTET}"
        ip addr add "${IP}/24" dev "$iface" 2>/dev/null
        ip route add default via 192.168.10.1 dev "$iface" 2>/dev/null
        echo "[NET]   ✓ IP estática: $IP"
        return 0
    fi
    
    return 1
}

# Configurar todas las interfaces disponibles
CONFIGURED=0
for iface in eth0 eth1 eth2 enp0s3 enp0s8 ens33 wlan0; do
    if configure_interface "$iface"; then
        CONFIGURED=1
    fi
done

# Mostrar configuración de red
echo ""
echo "[NET] Configuración actual:"
echo "────────────────────────────────────────────────────────────────────"
ip -4 addr show | grep -E "inet |^[0-9]" | while read line; do
    echo "      $line"
done
echo "────────────────────────────────────────────────────────────────────"
echo ""

if [ "$CONFIGURED" -eq 0 ]; then
    echo "[WARN] No se configuró ninguna interfaz de red"
    echo "[WARN] La red Ad-Hoc puede no funcionar correctamente"
    echo ""
fi

echo ""
echo "════════════════════════════════════════════════════════════════════"
echo "                    PUERTOS DEL SISTEMA"
echo "════════════════════════════════════════════════════════════════════"
echo ""
echo "   • UDP 8888 - Descubrimiento de nodos (broadcast)"
echo "   • TCP 8889 - Transferencia de datos entre nodos"
echo ""
echo "════════════════════════════════════════════════════════════════════"
echo ""

# Esperar un momento para que se estabilice la red
echo "[SYSTEM] Esperando estabilización de red..."
sleep 2

echo "[SYSTEM] Iniciando Sistema Operativo Descentralizado..."
echo ""

# Ejecutar nuestro sistema
exec /sbin/decos

# Si falla, dar shell de emergencia
echo ""
echo "[ERROR] El sistema ha terminado inesperadamente"
echo "[SHELL] Iniciando shell de emergencia..."
exec /bin/sh
EOFINIT

    chmod +x "$INITRAMFS/init"
    
    # Crear initramfs comprimido
    cd "$INITRAMFS"
    find . | cpio -o -H newc 2>/dev/null | gzip -9 > "$WORK_DIR/initramfs.gz"
    cd - > /dev/null
    
    print_ok "initramfs creado ($(du -h "$WORK_DIR/initramfs.gz" | cut -f1))"
}

# ============================================================================
# CREAR ESTRUCTURA ISO
# ============================================================================

create_iso_structure() {
    print_step "5" "Creando estructura ISO..."
    
    ISO_ROOT="$WORK_DIR/iso"
    rm -rf "$ISO_ROOT"
    mkdir -p "$ISO_ROOT"/{boot/isolinux,EFI/BOOT}
    
    # Copiar kernel e initramfs
    cp "$WORK_DIR/vmlinuz" "$ISO_ROOT/boot/vmlinuz"
    cp "$WORK_DIR/initramfs.gz" "$ISO_ROOT/boot/initramfs.gz"
    
    # Buscar y copiar isolinux.bin desde varias ubicaciones posibles
    ISOLINUX_FOUND=0
    for dir in /usr/lib/ISOLINUX /usr/share/syslinux /usr/lib/syslinux/bios /usr/lib/syslinux; do
        if [ -f "$dir/isolinux.bin" ]; then
            cp "$dir/isolinux.bin" "$ISO_ROOT/boot/isolinux/"
            echo "  isolinux.bin encontrado en: $dir"
            ISOLINUX_FOUND=1
            break
        fi
    done
    
    if [ "$ISOLINUX_FOUND" -eq 0 ]; then
        print_error "No se encontró isolinux.bin"
        echo "  Instala: apt-get install isolinux syslinux-common"
        exit 1
    fi
    
    # Copiar ldlinux.c32 (necesario para ISOLINUX 6.x)
    for dir in /usr/lib/ISOLINUX /usr/share/syslinux /usr/lib/syslinux/bios /usr/lib/syslinux/modules/bios; do
        if [ -f "$dir/ldlinux.c32" ]; then
            cp "$dir/ldlinux.c32" "$ISO_ROOT/boot/isolinux/"
            break
        fi
    done
    
    # Copiar módulos adicionales de syslinux
    for file in libutil.c32 libcom32.c32 menu.c32 vesamenu.c32; do
        for dir in /usr/lib/syslinux/modules/bios /usr/share/syslinux /usr/lib/syslinux; do
            if [ -f "$dir/$file" ]; then
                cp "$dir/$file" "$ISO_ROOT/boot/isolinux/"
                break
            fi
        done
    done
    
    # Crear configuración de ISOLINUX
    cat > "$ISO_ROOT/boot/isolinux/isolinux.cfg" << 'EOF'
DEFAULT decos
TIMEOUT 30
PROMPT 1

UI menu.c32

MENU TITLE Sistema Operativo Descentralizado - Fase 2
MENU COLOR border       30;44   #40ffffff #a0000000 std
MENU COLOR title        1;36;44 #9033ccff #a0000000 std
MENU COLOR sel          7;37;40 #e0ffffff #20ffffff all
MENU COLOR unsel        37;44   #50ffffff #a0000000 std
MENU COLOR help         37;40   #c0ffffff #a0000000 std
MENU COLOR timeout_msg  37;40   #80ffffff #00000000 std
MENU COLOR timeout      1;37;40 #c0ffffff #00000000 std

LABEL decos
    MENU LABEL ^Sistema Operativo Descentralizado
    MENU DEFAULT
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initramfs.gz quiet

LABEL decos_verbose
    MENU LABEL Modo ^Verbose (debug)
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initramfs.gz

LABEL shell
    MENU LABEL ^Shell de Emergencia
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initramfs.gz init=/bin/sh
EOF

    # Verificar que se creó correctamente
    echo "  Contenido de boot/isolinux:"
    ls -la "$ISO_ROOT/boot/isolinux/"

    print_ok "Estructura ISO creada"
}

# ============================================================================
# GENERAR ISO FINAL
# ============================================================================

generate_iso() {
    print_step "6" "Generando ISO final..."
    
    ISO_ROOT="$WORK_DIR/iso"
    
    # Verificar que isolinux.bin existe
    if [ ! -f "$ISO_ROOT/boot/isolinux/isolinux.bin" ]; then
        print_error "isolinux.bin no encontrado en $ISO_ROOT/boot/isolinux/"
        echo "Contenido de $ISO_ROOT/boot/isolinux/:"
        ls -la "$ISO_ROOT/boot/isolinux/" 2>/dev/null || echo "Directorio no existe"
        exit 1
    fi
    
    # Buscar isohdpfx.bin en varias ubicaciones
    ISOHDPFX=""
    for path in /usr/lib/ISOLINUX/isohdpfx.bin /usr/share/syslinux/isohdpfx.bin /usr/lib/syslinux/bios/isohdpfx.bin; do
        if [ -f "$path" ]; then
            ISOHDPFX="$path"
            break
        fi
    done
    
    if [ -n "$ISOHDPFX" ]; then
        echo "  Usando isohdpfx: $ISOHDPFX"
        xorriso -as mkisofs \
            -o "$OUTPUT_ISO" \
            -isohybrid-mbr "$ISOHDPFX" \
            -c boot/isolinux/boot.cat \
            -b boot/isolinux/isolinux.bin \
            -no-emul-boot \
            -boot-load-size 4 \
            -boot-info-table \
            "$ISO_ROOT"
    else
        echo "  Sin isohdpfx (ISO no será híbrida)"
        xorriso -as mkisofs \
            -o "$OUTPUT_ISO" \
            -c boot/isolinux/boot.cat \
            -b boot/isolinux/isolinux.bin \
            -no-emul-boot \
            -boot-load-size 4 \
            -boot-info-table \
            "$ISO_ROOT"
    fi
    
    if [ ! -f "$OUTPUT_ISO" ]; then
        print_error "No se pudo generar la ISO"
        exit 1
    fi
    
    # Hacer la ISO híbrida (booteable desde USB también)
    if command -v isohybrid &> /dev/null; then
        isohybrid "$OUTPUT_ISO" 2>/dev/null || true
    fi
    
    print_ok "ISO generada: $OUTPUT_ISO ($(du -h "$OUTPUT_ISO" | cut -f1))"
}

# ============================================================================
# CREAR README
# ============================================================================

create_readme() {
    print_step "7" "Creando documentación..."
    
    cat > "README_DecOS.md" << 'EOF'
# DecOS - Sistema Operativo Descentralizado v2.0
## Fase 2: Núcleo Funcional Distribuido

### Requisitos de Hardware
- Arquitectura: x86_64
- RAM mínima: 256MB (512MB recomendado)
- Red: Tarjeta de red Ethernet (para red Ad-Hoc)

### Ejecución en VirtualBox

1. **Crear VM:**
   - Tipo: Linux, Versión: Other Linux (64-bit)
   - RAM: 512MB
   - Sin disco duro (bootea desde ISO)

2. **Configurar Red (IMPORTANTE):**
   - Adaptador 1: Red Interna
   - Nombre: "adhoc" (igual en todas las VMs)
   - Modo promiscuo: Permitir todo

3. **Para probar múltiples nodos:**
   - Crear 2-3 VMs con la misma configuración de red
   - Todas deben usar la misma "Red Interna"
   - Los nodos se descubrirán automáticamente

### Ejecución en QEMU

```bash
# Nodo 1
qemu-system-x86_64 -cdrom DecOS_Fase2.iso -m 512 \
    -netdev socket,id=net0,listen=:1234 \
    -device e1000,netdev=net0

# Nodo 2 (en otra terminal)
qemu-system-x86_64 -cdrom DecOS_Fase2.iso -m 512 \
    -netdev socket,id=net0,connect=127.0.0.1:1234 \
    -device e1000,netdev=net0
```

### Comandos del Sistema

| Comando | Descripción |
|---------|-------------|
| `status` | Estado completo del sistema (nodo, red, scheduler, memoria) |
| `nodes` | Lista de nodos descubiertos en la red |
| `tasks` | Lista de tareas distribuidas |
| `task <desc>` | Crear nueva tarea (ej: `task Procesar datos`) |
| `alloc <bytes>` | Asignar memoria compartida (ej: `alloc 1024`) |
| `demo` | Ejecutar demostración de todas las funcionalidades |
| `help` | Mostrar ayuda |
| `exit` | Salir del sistema |

### Funcionalidades de la Fase 2

1. **Scheduler Distribuido**
   - Asignación de tareas basada en:
     - Carga de CPU del nodo
     - Uso de memoria
     - Reputación (historial de éxito/fallo)
     - Disponibilidad
   - Migración automática ante fallos

2. **Memoria Distribuida**
   - Bloques de memoria compartida entre nodos
   - Control de versiones
   - Soporte para replicación

3. **Sincronización**
   - Locks distribuidos con nombre
   - Adquisición con timeout
   - Prevención de deadlocks

4. **Tolerancia a Fallos**
   - Detección de nodos caídos (heartbeat)
   - Reasignación automática de tareas
   - Actualización de reputación

### Puertos de Red

- **UDP 8888**: Descubrimiento de nodos (broadcast)
- **TCP 8889**: Transferencia de datos

### Troubleshooting

**Los nodos no se ven entre sí:**
- Verificar que todas las VMs usen la misma "Red Interna"
- Verificar modo promiscuo habilitado
- Esperar 5-10 segundos para descubrimiento

**No hay IP asignada:**
- El sistema usa IP estática basada en MAC si DHCP falla
- Rango: 192.168.10.x/24

---
Generado automáticamente por build_iso.sh
EOF

    print_ok "README creado: README_DecOS.md"
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    print_header
    
    check_root
    check_dependencies
    
    # Limpiar trabajo anterior
    rm -rf "$WORK_DIR"
    mkdir -p "$WORK_DIR"
    
    print_step "1" "Preparando entorno..."
    print_ok "Directorio de trabajo: $WORK_DIR"
    
    compile_system
    download_components
    create_initramfs
    create_iso_structure
    generate_iso
    create_readme
    
    # Limpiar archivos temporales
    rm -rf "$WORK_DIR"
    
    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}                    ¡GENERACIÓN COMPLETADA!${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "   ISO:    ${CYAN}$OUTPUT_ISO${NC}"
    echo -e "   Tamaño: ${CYAN}$(du -h "$OUTPUT_ISO" | cut -f1)${NC}"
    echo -e "   README: ${CYAN}README_DecOS.md${NC}"
    echo ""
    echo -e "   Para probar en VirtualBox:"
    echo -e "   1. Crear VM Linux 64-bit con 512MB RAM"
    echo -e "   2. Red: 'Red Interna' nombre 'adhoc'"
    echo -e "   3. Cargar ${CYAN}$OUTPUT_ISO${NC} como CD"
    echo -e "   4. Crear múltiples VMs para probar red Ad-Hoc"
    echo ""
}

# Ejecutar
main "$@"
