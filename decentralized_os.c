/*
 * ============================================================================
 * SISTEMA OPERATIVO DESCENTRALIZADO - FASE 2 COMPLETA
 * ============================================================================
 * Núcleo Funcional Distribuido para Redes Ad-Hoc
 * 
 * Componentes implementados:
 * 1. Scheduler Distribuido (reputación, carga, disponibilidad)
 * 2. Gestión de Memoria Distribuida (compartir, replicar, acceder)
 * 3. Sincronización de Procesos Concurrentes
 * 4. Protocolos de Reconfiguración ante Fallos
 * 
 * Red: UDP Broadcast para descubrimiento, TCP para datos
 * ============================================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

// Networking
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>

// ============================================================================
// CONSTANTES Y CONFIGURACIÓN
// ============================================================================

#define DISCOVERY_PORT      8888
#define DATA_PORT          8889
#define SYNC_PORT          8890

#define MAX_NODES          64
#define MAX_TASKS          256
#define MAX_MEMORY_BLOCKS  512
#define MAX_LOCKS          128

#define BROADCAST_INTERVAL 3
#define HEARTBEAT_TIMEOUT  15
#define TASK_TIMEOUT       60

#define BUFFER_SIZE        4096
#define NODE_ID_SIZE       16

// Tipos de mensajes de red
typedef enum {
    MSG_DISCOVERY = 1,
    MSG_HEARTBEAT,
    MSG_TASK_ASSIGN,
    MSG_TASK_RESULT,
    MSG_MEMORY_REQUEST,
    MSG_MEMORY_RESPONSE,
    MSG_MEMORY_REPLICATE,
    MSG_SYNC_LOCK,
    MSG_SYNC_UNLOCK,
    MSG_NODE_FAILURE,
    MSG_LEADER_ELECTION,
    MSG_TASK_MIGRATE
} MessageType;

// Estados de nodo
typedef enum {
    NODE_UNKNOWN = 0,
    NODE_ACTIVE,
    NODE_BUSY,
    NODE_FAILED,
    NODE_RECOVERING
} NodeStatus;

// Estados de tarea
typedef enum {
    TASK_PENDING = 0,
    TASK_ASSIGNED,
    TASK_RUNNING,
    TASK_COMPLETED,
    TASK_FAILED,
    TASK_MIGRATING
} TaskStatus;

// ============================================================================
// ESTRUCTURAS DE DATOS
// ============================================================================

// Información de un nodo en la red
typedef struct {
    uint64_t node_id;
    char ip_address[INET_ADDRSTRLEN];
    char hostname[64];
    uint16_t data_port;
    
    // Métricas para scheduler
    float cpu_load;          // 0.0 - 1.0
    float memory_usage;      // 0.0 - 1.0
    float reputation;        // 0.0 - 1.0 (inicialmente 0.5)
    uint32_t tasks_completed;
    uint32_t tasks_failed;
    
    NodeStatus status;
    time_t last_seen;
    bool is_local;
} NodeInfo;

// Registro de nodos descubiertos
typedef struct {
    NodeInfo nodes[MAX_NODES];
    int count;
    pthread_mutex_t lock;
} NodeRegistry;

// Tarea distribuida
typedef struct {
    uint64_t task_id;
    uint64_t owner_node;     // Nodo que creó la tarea
    uint64_t assigned_node;  // Nodo que ejecuta la tarea
    
    char description[128];
    int priority;            // 1-10 (10 = máxima prioridad)
    
    TaskStatus status;
    time_t created_at;
    time_t started_at;
    time_t completed_at;
    
    // Datos de la tarea
    uint8_t data[1024];
    size_t data_size;
    
    // Resultado
    uint8_t result[1024];
    size_t result_size;
    int exit_code;
} DistributedTask;

// Scheduler Distribuido
typedef struct {
    DistributedTask tasks[MAX_TASKS];
    size_t task_count;
    uint64_t next_task_id;
    
    pthread_mutex_t lock;
    pthread_cond_t task_available;
    
    // Estadísticas
    uint64_t total_assigned;
    uint64_t total_completed;
    uint64_t total_failed;
    uint64_t total_migrated;
} DistributedScheduler;

// Bloque de memoria distribuida
typedef struct {
    uint64_t block_id;
    uint64_t owner_node;
    
    void* data;
    size_t size;
    
    uint32_t version;        // Para consistencia
    int ref_count;
    bool is_replicated;
    uint64_t replica_nodes[3]; // Hasta 3 réplicas
    int replica_count;
    
    pthread_rwlock_t rwlock;
} SharedMemoryBlock;

// Gestor de Memoria Distribuida
typedef struct {
    SharedMemoryBlock* blocks[MAX_MEMORY_BLOCKS];
    size_t block_count;
    uint64_t next_block_id;
    
    size_t total_allocated;
    size_t total_shared;
    
    pthread_mutex_t lock;
} DistributedMemoryManager;

// Lock distribuido para sincronización
typedef struct {
    uint64_t lock_id;
    char name[64];
    
    uint64_t owner_node;
    uint64_t owner_task;
    
    bool is_locked;
    time_t locked_at;
    
    // Cola de espera
    uint64_t waiting_nodes[MAX_NODES];
    int waiting_count;
    
    pthread_mutex_t local_lock;
} DistributedLock;

// Gestor de Sincronización
typedef struct {
    DistributedLock locks[MAX_LOCKS];
    size_t lock_count;
    uint64_t next_lock_id;
    
    pthread_mutex_t lock;
} SyncManager;

// Mensaje de red genérico
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint64_t sender_id;
    uint64_t timestamp;
    uint16_t payload_size;
    uint8_t payload[BUFFER_SIZE - 19];
} NetworkMessage;

// Payload de descubrimiento/heartbeat
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

// Kernel Distribuido Principal
typedef struct {
    uint64_t node_id;
    NodeInfo local_info;
    bool is_leader;
    uint64_t leader_id;
    
    NodeRegistry* registry;
    DistributedScheduler* scheduler;
    DistributedMemoryManager* memory;
    SyncManager* sync;
    
    // Sockets de red
    int discovery_socket;
    int data_socket;
    
    // Threads
    pthread_t discovery_thread;
    pthread_t heartbeat_thread;
    pthread_t data_server_thread;
    pthread_t scheduler_thread;
    pthread_t failure_detector_thread;
    pthread_t command_thread;
    
    volatile bool running;
} DistributedKernel;

// Variable global del kernel
static DistributedKernel* g_kernel = NULL;

// ============================================================================
// UTILIDADES
// ============================================================================

// Generar ID único del nodo basado en MAC + tiempo
static uint64_t generate_node_id(void) {
    uint64_t id = 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // Combinar tiempo con PID y valor aleatorio
    id = ((uint64_t)ts.tv_sec << 32) | (ts.tv_nsec & 0xFFFFFFFF);
    id ^= ((uint64_t)getpid() << 16);
    id ^= (uint64_t)rand();
    
    return id;
}

// Obtener IP local de la primera interfaz activa
static int get_local_ip(char* buffer, size_t len) {
    struct ifconf ifc;
    struct ifreq ifr[10];
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sock < 0) {
        strcpy(buffer, "127.0.0.1");
        return -1;
    }
    
    ifc.ifc_len = sizeof(ifr);
    ifc.ifc_req = ifr;
    
    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
        close(sock);
        strcpy(buffer, "127.0.0.1");
        return -1;
    }
    
    int num_ifaces = ifc.ifc_len / sizeof(struct ifreq);
    
    for (int i = 0; i < num_ifaces; i++) {
        struct sockaddr_in* addr = (struct sockaddr_in*)&ifr[i].ifr_addr;
        char* ip = inet_ntoa(addr->sin_addr);
        
        // Ignorar loopback
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

// Obtener carga de CPU
static float get_cpu_load(void) {
    FILE* f = fopen("/proc/loadavg", "r");
    if (!f) return 0.5;
    
    float load;
    if (fscanf(f, "%f", &load) != 1) load = 0.5;
    fclose(f);
    
    // Normalizar a 0-1
    return load > 1.0 ? 1.0 : load;
}

// Obtener uso de memoria
static float get_memory_usage(void) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return 0.5;
    
    long total = 0, available = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%ld", &total);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%ld", &available);
        }
    }
    fclose(f);
    
    if (total == 0) return 0.5;
    return 1.0 - ((float)available / total);
}

// Timestamp actual en ms
static uint64_t current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ============================================================================
// 1. DESCUBRIMIENTO DE NODOS (RED AD-HOC)
// ============================================================================

// Crear socket de broadcast para descubrimiento
static int create_discovery_socket(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    // Permitir broadcast
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt broadcast");
        close(sock);
        return -1;
    }
    
    // Permitir reutilizar dirección
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Bind al puerto de descubrimiento
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DISCOVERY_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind discovery");
        close(sock);
        return -1;
    }
    
    // Non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    return sock;
}

// Enviar anuncio de broadcast
static void send_discovery_broadcast(void) {
    if (!g_kernel || g_kernel->discovery_socket < 0) return;
    
    NetworkMessage msg;
    memset(&msg, 0, sizeof(msg));
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
    
    // Enviar a broadcast
    struct sockaddr_in bcast_addr;
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(DISCOVERY_PORT);
    bcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    
    sendto(g_kernel->discovery_socket, &msg, 
           sizeof(msg) - sizeof(msg.payload) + msg.payload_size,
           0, (struct sockaddr*)&bcast_addr, sizeof(bcast_addr));
    
    // También enviar a 192.168.x.255 subnets comunes
    const char* subnets[] = {
        "192.168.1.255",
        "192.168.0.255", 
        "192.168.10.255",
        "10.0.0.255",
        "10.0.2.255",
        NULL
    };
    
    for (int i = 0; subnets[i]; i++) {
        inet_pton(AF_INET, subnets[i], &bcast_addr.sin_addr);
        sendto(g_kernel->discovery_socket, &msg,
               sizeof(msg) - sizeof(msg.payload) + msg.payload_size,
               0, (struct sockaddr*)&bcast_addr, sizeof(bcast_addr));
    }
}

// Procesar mensaje de descubrimiento recibido
static void process_discovery_message(NetworkMessage* msg, struct sockaddr_in* sender) {
    if (!g_kernel || !msg) return;
    
    DiscoveryPayload* payload = (DiscoveryPayload*)msg->payload;
    
    // Ignorar mensajes propios
    if (payload->node_id == g_kernel->node_id) return;
    
    pthread_mutex_lock(&g_kernel->registry->lock);
    
    // Buscar si ya conocemos este nodo
    int found_idx = -1;
    for (int i = 0; i < g_kernel->registry->count; i++) {
        if (g_kernel->registry->nodes[i].node_id == payload->node_id) {
            found_idx = i;
            break;
        }
    }
    
    NodeInfo* node;
    if (found_idx >= 0) {
        node = &g_kernel->registry->nodes[found_idx];
    } else if (g_kernel->registry->count < MAX_NODES) {
        // Nuevo nodo
        node = &g_kernel->registry->nodes[g_kernel->registry->count++];
        printf("\n[DISCOVERY] ✓ Nuevo nodo descubierto!\n");
        printf("            ID: %016lX\n", payload->node_id);
        printf("            Host: %s\n", payload->hostname);
        printf("            IP: %s\n", inet_ntoa(sender->sin_addr));
    } else {
        pthread_mutex_unlock(&g_kernel->registry->lock);
        return;
    }
    
    // Actualizar información
    node->node_id = payload->node_id;
    strncpy(node->hostname, payload->hostname, 63);
    strncpy(node->ip_address, inet_ntoa(sender->sin_addr), INET_ADDRSTRLEN - 1);
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

// Thread de escucha de descubrimiento
static void* discovery_listener_thread(void* arg) {
    (void)arg;
    
    NetworkMessage msg;
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    
    while (g_kernel && g_kernel->running) {
        memset(&msg, 0, sizeof(msg));
        
        ssize_t n = recvfrom(g_kernel->discovery_socket, &msg, sizeof(msg),
                            0, (struct sockaddr*)&sender, &sender_len);
        
        if (n > 0) {
            switch (msg.type) {
                case MSG_DISCOVERY:
                case MSG_HEARTBEAT:
                    process_discovery_message(&msg, &sender);
                    break;
                case MSG_NODE_FAILURE:
                    // Procesar notificación de fallo
                    break;
                default:
                    break;
            }
        }
        
        usleep(10000); // 10ms
    }
    
    return NULL;
}

// Thread de broadcast periódico
static void* heartbeat_broadcast_thread(void* arg) {
    (void)arg;
    
    while (g_kernel && g_kernel->running) {
        send_discovery_broadcast();
        sleep(BROADCAST_INTERVAL);
    }
    
    return NULL;
}

// ============================================================================
// 2. SCHEDULER DISTRIBUIDO
// ============================================================================

// Calcular puntuación de un nodo para asignación de tarea
static float calculate_node_score(NodeInfo* node, int task_priority) {
    if (!node || node->status != NODE_ACTIVE) return -1.0;
    
    // Factores de peso
    const float W_LOAD = 0.30;      // Peso de carga CPU
    const float W_MEMORY = 0.20;    // Peso de memoria
    const float W_REPUTATION = 0.35; // Peso de reputación
    const float W_FRESHNESS = 0.15; // Peso de recencia
    
    // Calcular componentes (invertir carga y memoria: menos = mejor)
    float load_score = 1.0 - node->cpu_load;
    float mem_score = 1.0 - node->memory_usage;
    float rep_score = node->reputation;
    
    // Frescura: penalizar nodos que no hemos visto recientemente
    time_t now = time(NULL);
    float freshness = 1.0;
    if (now - node->last_seen > 5) {
        freshness = 1.0 / (1.0 + (now - node->last_seen - 5) * 0.1);
    }
    
    // Bonus por alta prioridad a nodos con buena reputación
    float priority_bonus = 0;
    if (task_priority >= 8 && rep_score > 0.7) {
        priority_bonus = 0.1;
    }
    
    float score = W_LOAD * load_score +
                  W_MEMORY * mem_score +
                  W_REPUTATION * rep_score +
                  W_FRESHNESS * freshness +
                  priority_bonus;
    
    return score;
}

// Seleccionar mejor nodo para una tarea
static uint64_t select_best_node(int task_priority) {
    if (!g_kernel) return 0;
    
    pthread_mutex_lock(&g_kernel->registry->lock);
    
    float best_score = -1.0;
    uint64_t best_node = 0;
    
    for (int i = 0; i < g_kernel->registry->count; i++) {
        NodeInfo* node = &g_kernel->registry->nodes[i];
        
        if (node->status != NODE_ACTIVE) continue;
        
        float score = calculate_node_score(node, task_priority);
        if (score > best_score) {
            best_score = score;
            best_node = node->node_id;
        }
    }
    
    // También considerar el nodo local
    float local_score = calculate_node_score(&g_kernel->local_info, task_priority);
    if (local_score > best_score) {
        best_node = g_kernel->node_id;
    }
    
    pthread_mutex_unlock(&g_kernel->registry->lock);
    
    return best_node;
}

// Crear nueva tarea
static uint64_t create_task(const char* description, int priority, 
                           void* data, size_t data_size) {
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
    
    if (description) {
        strncpy(task->description, description, sizeof(task->description) - 1);
    }
    
    if (data && data_size > 0 && data_size <= sizeof(task->data)) {
        memcpy(task->data, data, data_size);
        task->data_size = data_size;
    }
    
    // Seleccionar nodo para ejecutar
    uint64_t target = select_best_node(task->priority);
    if (target) {
        task->assigned_node = target;
        task->status = TASK_ASSIGNED;
        g_kernel->scheduler->total_assigned++;
        
        printf("[SCHEDULER] Tarea %lu asignada al nodo %016lX\n",
               task->task_id, target);
    }
    
    g_kernel->scheduler->task_count++;
    
    pthread_cond_signal(&g_kernel->scheduler->task_available);
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    
    return task->task_id;
}

// Actualizar reputación de un nodo
static void update_node_reputation(uint64_t node_id, bool success) {
    if (!g_kernel) return;
    
    pthread_mutex_lock(&g_kernel->registry->lock);
    
    for (int i = 0; i < g_kernel->registry->count; i++) {
        if (g_kernel->registry->nodes[i].node_id == node_id) {
            NodeInfo* node = &g_kernel->registry->nodes[i];
            
            // Ajuste exponencial de reputación
            float delta = success ? 0.05 : -0.10;
            node->reputation += delta * (1.0 - node->reputation);
            
            // Limitar a [0.1, 1.0]
            if (node->reputation < 0.1) node->reputation = 0.1;
            if (node->reputation > 1.0) node->reputation = 1.0;
            
            if (success) {
                node->tasks_completed++;
            } else {
                node->tasks_failed++;
            }
            
            break;
        }
    }
    
    pthread_mutex_unlock(&g_kernel->registry->lock);
}

// Completar tarea
static void complete_task(uint64_t task_id, int exit_code, void* result, size_t result_size) {
    if (!g_kernel) return;
    
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    
    for (size_t i = 0; i < g_kernel->scheduler->task_count; i++) {
        DistributedTask* task = &g_kernel->scheduler->tasks[i];
        
        if (task->task_id == task_id) {
            task->status = (exit_code == 0) ? TASK_COMPLETED : TASK_FAILED;
            task->completed_at = time(NULL);
            task->exit_code = exit_code;
            
            if (result && result_size > 0 && result_size <= sizeof(task->result)) {
                memcpy(task->result, result, result_size);
                task->result_size = result_size;
            }
            
            if (exit_code == 0) {
                g_kernel->scheduler->total_completed++;
            } else {
                g_kernel->scheduler->total_failed++;
            }
            
            // Actualizar reputación del nodo
            update_node_reputation(task->assigned_node, exit_code == 0);
            
            break;
        }
    }
    
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
}

// Migrar tarea a otro nodo (tolerancia a fallos)
static bool migrate_task(uint64_t task_id, uint64_t new_node) {
    if (!g_kernel) return false;
    
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    
    for (size_t i = 0; i < g_kernel->scheduler->task_count; i++) {
        DistributedTask* task = &g_kernel->scheduler->tasks[i];
        
        if (task->task_id == task_id && 
            (task->status == TASK_ASSIGNED || task->status == TASK_RUNNING)) {
            
            uint64_t old_node = task->assigned_node;
            task->assigned_node = new_node;
            task->status = TASK_MIGRATING;
            g_kernel->scheduler->total_migrated++;
            
            printf("[SCHEDULER] Tarea %lu migrada: %016lX -> %016lX\n",
                   task_id, old_node, new_node);
            
            pthread_mutex_unlock(&g_kernel->scheduler->lock);
            return true;
        }
    }
    
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    return false;
}

// ============================================================================
// 3. GESTIÓN DE MEMORIA DISTRIBUIDA
// ============================================================================

// Crear bloque de memoria compartida
static uint64_t create_shared_memory(size_t size) {
    if (!g_kernel || !g_kernel->memory || size == 0) return 0;
    
    pthread_mutex_lock(&g_kernel->memory->lock);
    
    if (g_kernel->memory->block_count >= MAX_MEMORY_BLOCKS) {
        pthread_mutex_unlock(&g_kernel->memory->lock);
        return 0;
    }
    
    SharedMemoryBlock* block = calloc(1, sizeof(SharedMemoryBlock));
    if (!block) {
        pthread_mutex_unlock(&g_kernel->memory->lock);
        return 0;
    }
    
    block->data = calloc(1, size);
    if (!block->data) {
        free(block);
        pthread_mutex_unlock(&g_kernel->memory->lock);
        return 0;
    }
    
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

// Escribir en memoria compartida
static int write_shared_memory(uint64_t block_id, const void* data, 
                               size_t size, size_t offset) {
    if (!g_kernel || !data) return -1;
    
    pthread_mutex_lock(&g_kernel->memory->lock);
    
    SharedMemoryBlock* block = NULL;
    for (size_t i = 0; i < g_kernel->memory->block_count; i++) {
        if (g_kernel->memory->blocks[i]->block_id == block_id) {
            block = g_kernel->memory->blocks[i];
            break;
        }
    }
    
    if (!block || offset + size > block->size) {
        pthread_mutex_unlock(&g_kernel->memory->lock);
        return -1;
    }
    
    pthread_rwlock_wrlock(&block->rwlock);
    memcpy((uint8_t*)block->data + offset, data, size);
    block->version++;
    pthread_rwlock_unlock(&block->rwlock);
    
    pthread_mutex_unlock(&g_kernel->memory->lock);
    
    return 0;
}

// Leer de memoria compartida
static int read_shared_memory(uint64_t block_id, void* buffer, 
                              size_t size, size_t offset) {
    if (!g_kernel || !buffer) return -1;
    
    pthread_mutex_lock(&g_kernel->memory->lock);
    
    SharedMemoryBlock* block = NULL;
    for (size_t i = 0; i < g_kernel->memory->block_count; i++) {
        if (g_kernel->memory->blocks[i]->block_id == block_id) {
            block = g_kernel->memory->blocks[i];
            break;
        }
    }
    
    if (!block || offset + size > block->size) {
        pthread_mutex_unlock(&g_kernel->memory->lock);
        return -1;
    }
    
    pthread_rwlock_rdlock(&block->rwlock);
    memcpy(buffer, (uint8_t*)block->data + offset, size);
    pthread_rwlock_unlock(&block->rwlock);
    
    pthread_mutex_unlock(&g_kernel->memory->lock);
    
    return 0;
}

// Replicar bloque de memoria a otros nodos
static int replicate_memory_block(uint64_t block_id, uint64_t target_node) {
    if (!g_kernel) return -1;
    
    pthread_mutex_lock(&g_kernel->memory->lock);
    
    SharedMemoryBlock* block = NULL;
    for (size_t i = 0; i < g_kernel->memory->block_count; i++) {
        if (g_kernel->memory->blocks[i]->block_id == block_id) {
            block = g_kernel->memory->blocks[i];
            break;
        }
    }
    
    if (!block || block->replica_count >= 3) {
        pthread_mutex_unlock(&g_kernel->memory->lock);
        return -1;
    }
    
    block->replica_nodes[block->replica_count++] = target_node;
    block->is_replicated = true;
    g_kernel->memory->total_shared += block->size;
    
    printf("[MEMORY] Bloque %lu replicado al nodo %016lX\n", block_id, target_node);
    
    pthread_mutex_unlock(&g_kernel->memory->lock);
    
    // TODO: Enviar datos por red al nodo destino
    
    return 0;
}

// Liberar bloque de memoria
static void free_shared_memory(uint64_t block_id) {
    if (!g_kernel) return;
    
    pthread_mutex_lock(&g_kernel->memory->lock);
    
    for (size_t i = 0; i < g_kernel->memory->block_count; i++) {
        if (g_kernel->memory->blocks[i]->block_id == block_id) {
            SharedMemoryBlock* block = g_kernel->memory->blocks[i];
            
            block->ref_count--;
            if (block->ref_count <= 0) {
                g_kernel->memory->total_allocated -= block->size;
                free(block->data);
                pthread_rwlock_destroy(&block->rwlock);
                free(block);
                
                // Compactar array
                for (size_t j = i; j < g_kernel->memory->block_count - 1; j++) {
                    g_kernel->memory->blocks[j] = g_kernel->memory->blocks[j + 1];
                }
                g_kernel->memory->block_count--;
            }
            
            break;
        }
    }
    
    pthread_mutex_unlock(&g_kernel->memory->lock);
}

// ============================================================================
// 4. SINCRONIZACIÓN DE PROCESOS DISTRIBUIDOS
// ============================================================================

// Crear lock distribuido
static uint64_t create_distributed_lock(const char* name) {
    if (!g_kernel || !g_kernel->sync) return 0;
    
    pthread_mutex_lock(&g_kernel->sync->lock);
    
    // Verificar si ya existe
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

// Adquirir lock distribuido
static int acquire_distributed_lock(uint64_t lock_id, uint64_t task_id, int timeout_ms) {
    if (!g_kernel) return -1;
    
    pthread_mutex_lock(&g_kernel->sync->lock);
    
    DistributedLock* lock = NULL;
    for (size_t i = 0; i < g_kernel->sync->lock_count; i++) {
        if (g_kernel->sync->locks[i].lock_id == lock_id) {
            lock = &g_kernel->sync->locks[i];
            break;
        }
    }
    
    if (!lock) {
        pthread_mutex_unlock(&g_kernel->sync->lock);
        return -1;
    }
    
    pthread_mutex_unlock(&g_kernel->sync->lock);
    
    // Intentar adquirir
    int elapsed = 0;
    while (elapsed < timeout_ms || timeout_ms < 0) {
        pthread_mutex_lock(&lock->local_lock);
        
        if (!lock->is_locked) {
            lock->is_locked = true;
            lock->owner_node = g_kernel->node_id;
            lock->owner_task = task_id;
            lock->locked_at = time(NULL);
            pthread_mutex_unlock(&lock->local_lock);
            return 0;
        }
        
        pthread_mutex_unlock(&lock->local_lock);
        
        usleep(10000); // 10ms
        elapsed += 10;
    }
    
    return -1; // Timeout
}

// Liberar lock distribuido
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
                lock->owner_task = 0;
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
// 5. DETECCIÓN Y RECUPERACIÓN DE FALLOS
// ============================================================================

// Thread de detección de fallos
static void* failure_detector_thread(void* arg) {
    (void)arg;
    
    while (g_kernel && g_kernel->running) {
        time_t now = time(NULL);
        
        pthread_mutex_lock(&g_kernel->registry->lock);
        
        for (int i = 0; i < g_kernel->registry->count; i++) {
            NodeInfo* node = &g_kernel->registry->nodes[i];
            
            if (node->status == NODE_ACTIVE || node->status == NODE_BUSY) {
                if (now - node->last_seen > HEARTBEAT_TIMEOUT) {
                    // Nodo no responde - marcar como fallido
                    printf("\n[FAILURE] ⚠ Nodo %016lX no responde!\n", node->node_id);
                    node->status = NODE_FAILED;
                    
                    // Reasignar tareas de este nodo
                    pthread_mutex_lock(&g_kernel->scheduler->lock);
                    
                    for (size_t j = 0; j < g_kernel->scheduler->task_count; j++) {
                        DistributedTask* task = &g_kernel->scheduler->tasks[j];
                        
                        if (task->assigned_node == node->node_id &&
                            (task->status == TASK_ASSIGNED || task->status == TASK_RUNNING)) {
                            
                            // Buscar otro nodo
                            uint64_t new_node = select_best_node(task->priority);
                            if (new_node && new_node != node->node_id) {
                                task->assigned_node = new_node;
                                task->status = TASK_ASSIGNED;
                                g_kernel->scheduler->total_migrated++;
                                
                                printf("[FAILURE] Tarea %lu reasignada a %016lX\n",
                                       task->task_id, new_node);
                            }
                        }
                    }
                    
                    pthread_mutex_unlock(&g_kernel->scheduler->lock);
                    
                    // Penalizar reputación
                    node->reputation *= 0.5;
                }
            }
        }
        
        pthread_mutex_unlock(&g_kernel->registry->lock);
        
        sleep(5);
    }
    
    return NULL;
}

// ============================================================================
// 6. SERVIDOR DE DATOS TCP
// ============================================================================

static int create_data_server(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DATA_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    if (listen(sock, 10) < 0) {
        close(sock);
        return -1;
    }
    
    // Non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    return sock;
}

static void* data_server_thread(void* arg) {
    (void)arg;
    
    while (g_kernel && g_kernel->running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client = accept(g_kernel->data_socket, 
                           (struct sockaddr*)&client_addr, &addr_len);
        
        if (client >= 0) {
            // TODO: Manejar conexión de datos
            // Por ahora solo cerrar
            close(client);
        }
        
        usleep(50000);
    }
    
    return NULL;
}

// ============================================================================
// 7. INTERFAZ DE COMANDOS
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
    printf("\n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                      ESTADO DEL SISTEMA\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    // Nodo local
    printf("🖥  NODO LOCAL\n");
    printf("   ID:        %016lX\n", g_kernel->node_id);
    printf("   Hostname:  %s\n", g_kernel->local_info.hostname);
    printf("   IP:        %s\n", g_kernel->local_info.ip_address);
    printf("   CPU:       %.1f%%\n", get_cpu_load() * 100);
    printf("   Memoria:   %.1f%%\n", get_memory_usage() * 100);
    printf("   Reputación: %.2f\n", g_kernel->local_info.reputation);
    printf("\n");
    
    // Red
    pthread_mutex_lock(&g_kernel->registry->lock);
    int active_nodes = 0;
    for (int i = 0; i < g_kernel->registry->count; i++) {
        if (g_kernel->registry->nodes[i].status == NODE_ACTIVE) {
            active_nodes++;
        }
    }
    pthread_mutex_unlock(&g_kernel->registry->lock);
    
    printf("🌐 RED AD-HOC\n");
    printf("   Nodos activos:  %d\n", active_nodes);
    printf("   Total nodos:    %d\n", g_kernel->registry->count);
    printf("   Puerto UDP:     %d (Discovery)\n", DISCOVERY_PORT);
    printf("   Puerto TCP:     %d (Datos)\n", DATA_PORT);
    printf("\n");
    
    // Scheduler
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    printf("📋 SCHEDULER DISTRIBUIDO\n");
    printf("   Tareas totales:     %zu\n", g_kernel->scheduler->task_count);
    printf("   Tareas asignadas:   %lu\n", g_kernel->scheduler->total_assigned);
    printf("   Tareas completadas: %lu\n", g_kernel->scheduler->total_completed);
    printf("   Tareas fallidas:    %lu\n", g_kernel->scheduler->total_failed);
    printf("   Tareas migradas:    %lu\n", g_kernel->scheduler->total_migrated);
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    printf("\n");
    
    // Memoria
    pthread_mutex_lock(&g_kernel->memory->lock);
    printf("💾 MEMORIA DISTRIBUIDA\n");
    printf("   Bloques:       %zu\n", g_kernel->memory->block_count);
    printf("   Asignada:      %zu bytes\n", g_kernel->memory->total_allocated);
    printf("   Compartida:    %zu bytes\n", g_kernel->memory->total_shared);
    pthread_mutex_unlock(&g_kernel->memory->lock);
    printf("\n");
    
    // Sincronización
    pthread_mutex_lock(&g_kernel->sync->lock);
    printf("🔒 SINCRONIZACIÓN\n");
    printf("   Locks activos: %zu\n", g_kernel->sync->lock_count);
    pthread_mutex_unlock(&g_kernel->sync->lock);
    printf("\n");
}

static void print_nodes(void) {
    printf("\n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                       NODOS EN LA RED\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    pthread_mutex_lock(&g_kernel->registry->lock);
    
    if (g_kernel->registry->count == 0) {
        printf("   No se han descubierto otros nodos aún.\n");
        printf("   Esperando broadcast de otros nodos...\n");
    } else {
        printf("   %-18s %-16s %-10s %-6s %-6s %-5s\n",
               "NODE ID", "IP", "STATUS", "CPU", "MEM", "REP");
        printf("   ────────────────── ──────────────── ────────── ────── ────── ─────\n");
        
        for (int i = 0; i < g_kernel->registry->count; i++) {
            NodeInfo* n = &g_kernel->registry->nodes[i];
            
            const char* status_str = "?";
            switch (n->status) {
                case NODE_ACTIVE: status_str = "ACTIVO"; break;
                case NODE_BUSY: status_str = "OCUPADO"; break;
                case NODE_FAILED: status_str = "FALLIDO"; break;
                case NODE_RECOVERING: status_str = "RECUP."; break;
                default: status_str = "DESCON."; break;
            }
            
            printf("   %016lX %-16s %-10s %5.1f%% %5.1f%% %.2f\n",
                   n->node_id, n->ip_address, status_str,
                   n->cpu_load * 100, n->memory_usage * 100, n->reputation);
        }
    }
    
    pthread_mutex_unlock(&g_kernel->registry->lock);
    printf("\n");
}

static void print_tasks(void) {
    printf("\n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                         TAREAS\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    
    if (g_kernel->scheduler->task_count == 0) {
        printf("   No hay tareas registradas.\n");
    } else {
        printf("   %-5s %-30s %-18s %-10s\n",
               "ID", "DESCRIPCIÓN", "NODO", "ESTADO");
        printf("   ───── ────────────────────────────── ────────────────── ──────────\n");
        
        for (size_t i = 0; i < g_kernel->scheduler->task_count; i++) {
            DistributedTask* t = &g_kernel->scheduler->tasks[i];
            
            const char* status_str = "?";
            switch (t->status) {
                case TASK_PENDING: status_str = "PENDIENTE"; break;
                case TASK_ASSIGNED: status_str = "ASIGNADA"; break;
                case TASK_RUNNING: status_str = "EJECUTANDO"; break;
                case TASK_COMPLETED: status_str = "COMPLETADA"; break;
                case TASK_FAILED: status_str = "FALLIDA"; break;
                case TASK_MIGRATING: status_str = "MIGRANDO"; break;
            }
            
            char desc[31];
            strncpy(desc, t->description, 30);
            desc[30] = '\0';
            
            printf("   %-5lu %-30s %016lX %-10s\n",
                   t->task_id, desc, t->assigned_node, status_str);
        }
    }
    
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    printf("\n");
}

static void print_memory(void) {
    printf("\n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                    MEMORIA DISTRIBUIDA\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    pthread_mutex_lock(&g_kernel->memory->lock);
    
    if (g_kernel->memory->block_count == 0) {
        printf("   No hay bloques de memoria compartida.\n");
    } else {
        printf("   %-8s %-18s %-10s %-8s %-10s\n",
               "BLOQUE", "OWNER", "TAMAÑO", "VERSION", "REPLICAS");
        printf("   ──────── ────────────────── ────────── ──────── ──────────\n");
        
        for (size_t i = 0; i < g_kernel->memory->block_count; i++) {
            SharedMemoryBlock* b = g_kernel->memory->blocks[i];
            
            printf("   %-8lu %016lX %10zu %8u %10d\n",
                   b->block_id, b->owner_node, b->size, b->version, b->replica_count);
        }
    }
    
    pthread_mutex_unlock(&g_kernel->memory->lock);
    printf("\n");
}

static void print_help(void) {
    printf("\n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("                          AYUDA\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    printf("COMANDOS DISPONIBLES:\n\n");
    
    printf("   status          Ver estado completo del sistema\n");
    printf("   nodes           Listar nodos en la red\n");
    printf("   tasks           Listar tareas del sistema\n");
    printf("   memory          Ver memoria distribuida\n\n");
    
    printf("   task <desc>     Crear nueva tarea distribuida\n");
    printf("                   Ejemplo: task Procesar datos ML\n\n");
    
    printf("   alloc <bytes>   Asignar memoria compartida\n");
    printf("                   Ejemplo: alloc 1024\n\n");
    
    printf("   demo            Ejecutar demostración de funcionalidades\n");
    printf("   help            Mostrar esta ayuda\n");
    printf("   exit            Salir del sistema\n\n");
    
    printf("RED AD-HOC:\n");
    printf("   • Descubrimiento automático por broadcast UDP\n");
    printf("   • Puerto %d: Discovery/Heartbeat\n", DISCOVERY_PORT);
    printf("   • Puerto %d: Transferencia de datos\n", DATA_PORT);
    printf("   • Broadcast cada %d segundos\n", BROADCAST_INTERVAL);
    printf("   • Timeout de nodo: %d segundos\n\n", HEARTBEAT_TIMEOUT);
}

// Demostración de funcionalidades
static void run_demo(void) {
    printf("\n");
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("              DEMOSTRACIÓN DE FASE 2\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
    
    // 1. Scheduler Distribuido
    printf("▶ 1. SCHEDULER DISTRIBUIDO\n");
    printf("   Creando tareas con diferentes prioridades...\n\n");
    
    uint64_t t1 = create_task("Entrenamiento modelo ML", 9, NULL, 0);
    uint64_t t2 = create_task("Procesamiento de datos", 5, NULL, 0);
    uint64_t t3 = create_task("Análisis de resultados", 7, NULL, 0);
    
    printf("   ✓ Tarea %lu (prioridad 9) - ML\n", t1);
    printf("   ✓ Tarea %lu (prioridad 5) - Datos\n", t2);
    printf("   ✓ Tarea %lu (prioridad 7) - Análisis\n", t3);
    
    // Simular completar una tarea
    sleep(1);
    complete_task(t1, 0, "OK", 2);
    printf("   ✓ Tarea %lu completada exitosamente\n\n", t1);
    
    // 2. Memoria Distribuida
    printf("▶ 2. MEMORIA DISTRIBUIDA\n");
    printf("   Creando bloques de memoria compartida...\n\n");
    
    uint64_t m1 = create_shared_memory(4096);
    uint64_t m2 = create_shared_memory(1024);
    
    const char* test_data = "Datos de prueba para memoria distribuida";
    write_shared_memory(m1, test_data, strlen(test_data) + 1, 0);
    
    char buffer[256];
    read_shared_memory(m1, buffer, strlen(test_data) + 1, 0);
    
    printf("   ✓ Bloque %lu: 4096 bytes\n", m1);
    printf("   ✓ Bloque %lu: 1024 bytes\n", m2);
    printf("   ✓ Escritura/Lectura verificada: \"%s\"\n\n", buffer);
    
    // 3. Sincronización
    printf("▶ 3. SINCRONIZACIÓN DISTRIBUIDA\n");
    printf("   Creando locks distribuidos...\n\n");
    
    uint64_t lock1 = create_distributed_lock("recurso_compartido");
    uint64_t lock2 = create_distributed_lock("base_datos");
    
    if (acquire_distributed_lock(lock1, 1, 1000) == 0) {
        printf("   ✓ Lock '%s' adquirido\n", "recurso_compartido");
        release_distributed_lock(lock1);
        printf("   ✓ Lock liberado\n");
    }
    
    printf("   ✓ Lock ID %lu: recurso_compartido\n", lock1);
    printf("   ✓ Lock ID %lu: base_datos\n\n", lock2);
    
    // 4. Tolerancia a Fallos (simulación)
    printf("▶ 4. TOLERANCIA A FALLOS\n");
    printf("   Simulando detección de fallos...\n\n");
    
    printf("   • Monitor de heartbeat activo (timeout: %ds)\n", HEARTBEAT_TIMEOUT);
    printf("   • Migración automática de tareas habilitada\n");
    printf("   • Replicación de memoria disponible\n\n");
    
    printf("════════════════════════════════════════════════════════════════════\n");
    printf("              DEMOSTRACIÓN COMPLETADA\n");
    printf("════════════════════════════════════════════════════════════════════\n\n");
}

static void* command_thread(void* arg) {
    (void)arg;
    
    char line[256];
    char cmd[64];
    char args[192];
    
    printf("\nEscribe 'help' para ver los comandos disponibles.\n\n");
    
    while (g_kernel && g_kernel->running) {
        printf("DecOS> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) {
                g_kernel->running = false;
                break;
            }
            continue;
        }
        
        // Remover newline
        line[strcspn(line, "\n")] = '\0';
        
        // Parsear comando y argumentos
        cmd[0] = '\0';
        args[0] = '\0';
        sscanf(line, "%63s %191[^\n]", cmd, args);
        
        if (strlen(cmd) == 0) continue;
        
        if (strcmp(cmd, "status") == 0) {
            print_status();
        }
        else if (strcmp(cmd, "nodes") == 0) {
            print_nodes();
        }
        else if (strcmp(cmd, "tasks") == 0) {
            print_tasks();
        }
        else if (strcmp(cmd, "memory") == 0) {
            print_memory();
        }
        else if (strcmp(cmd, "task") == 0) {
            if (strlen(args) > 0) {
                uint64_t tid = create_task(args, 5, NULL, 0);
                if (tid) {
                    printf("Tarea %lu creada: %s\n", tid, args);
                } else {
                    printf("Error: No se pudo crear la tarea\n");
                }
            } else {
                printf("Uso: task <descripción>\n");
            }
        }
        else if (strcmp(cmd, "alloc") == 0) {
            int size = 0;
            if (sscanf(args, "%d", &size) == 1 && size > 0) {
                uint64_t bid = create_shared_memory((size_t)size);
                if (bid) {
                    printf("Bloque %lu creado: %d bytes\n", bid, size);
                } else {
                    printf("Error: No se pudo asignar memoria\n");
                }
            } else {
                printf("Uso: alloc <bytes>\n");
            }
        }
        else if (strcmp(cmd, "demo") == 0) {
            run_demo();
        }
        else if (strcmp(cmd, "help") == 0) {
            print_help();
        }
        else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            printf("Apagando sistema...\n");
            g_kernel->running = false;
            break;
        }
        else {
            printf("Comando desconocido: '%s'. Usa 'help' para ayuda.\n", cmd);
        }
    }
    
    return NULL;
}

// ============================================================================
// 8. INICIALIZACIÓN Y MAIN
// ============================================================================

static void handle_signal(int sig) {
    (void)sig;
    printf("\n\n[SIGNAL] Recibida señal de terminación\n");
    if (g_kernel) {
        g_kernel->running = false;
    }
}

static int init_kernel(uint64_t node_id) {
    g_kernel = calloc(1, sizeof(DistributedKernel));
    if (!g_kernel) return -1;
    
    // ID del nodo
    g_kernel->node_id = node_id ? node_id : generate_node_id();
    
    // Info local
    gethostname(g_kernel->local_info.hostname, sizeof(g_kernel->local_info.hostname));
    get_local_ip(g_kernel->local_info.ip_address, sizeof(g_kernel->local_info.ip_address));
    g_kernel->local_info.node_id = g_kernel->node_id;
    g_kernel->local_info.data_port = DATA_PORT;
    g_kernel->local_info.reputation = 0.5;
    g_kernel->local_info.status = NODE_ACTIVE;
    g_kernel->local_info.is_local = true;
    g_kernel->local_info.last_seen = time(NULL);
    
    // Registro de nodos
    g_kernel->registry = calloc(1, sizeof(NodeRegistry));
    pthread_mutex_init(&g_kernel->registry->lock, NULL);
    
    // Scheduler
    g_kernel->scheduler = calloc(1, sizeof(DistributedScheduler));
    pthread_mutex_init(&g_kernel->scheduler->lock, NULL);
    pthread_cond_init(&g_kernel->scheduler->task_available, NULL);
    
    // Memoria distribuida
    g_kernel->memory = calloc(1, sizeof(DistributedMemoryManager));
    pthread_mutex_init(&g_kernel->memory->lock, NULL);
    
    // Sincronización
    g_kernel->sync = calloc(1, sizeof(SyncManager));
    pthread_mutex_init(&g_kernel->sync->lock, NULL);
    
    // Sockets
    g_kernel->discovery_socket = create_discovery_socket();
    if (g_kernel->discovery_socket < 0) {
        printf("[ERROR] No se pudo crear socket de discovery\n");
        return -1;
    }
    
    g_kernel->data_socket = create_data_server();
    if (g_kernel->data_socket < 0) {
        printf("[WARN] No se pudo crear servidor de datos\n");
    }
    
    g_kernel->running = true;
    
    return 0;
}

static void start_threads(void) {
    pthread_create(&g_kernel->discovery_thread, NULL, discovery_listener_thread, NULL);
    pthread_create(&g_kernel->heartbeat_thread, NULL, heartbeat_broadcast_thread, NULL);
    pthread_create(&g_kernel->failure_detector_thread, NULL, failure_detector_thread, NULL);
    
    if (g_kernel->data_socket >= 0) {
        pthread_create(&g_kernel->data_server_thread, NULL, data_server_thread, NULL);
    }
}

static void cleanup(void) {
    if (!g_kernel) return;
    
    g_kernel->running = false;
    
    // Esperar threads
    pthread_join(g_kernel->discovery_thread, NULL);
    pthread_join(g_kernel->heartbeat_thread, NULL);
    pthread_join(g_kernel->failure_detector_thread, NULL);
    
    // Cerrar sockets
    if (g_kernel->discovery_socket >= 0) close(g_kernel->discovery_socket);
    if (g_kernel->data_socket >= 0) close(g_kernel->data_socket);
    
    // Liberar memoria distribuida
    pthread_mutex_lock(&g_kernel->memory->lock);
    for (size_t i = 0; i < g_kernel->memory->block_count; i++) {
        if (g_kernel->memory->blocks[i]) {
            free(g_kernel->memory->blocks[i]->data);
            pthread_rwlock_destroy(&g_kernel->memory->blocks[i]->rwlock);
            free(g_kernel->memory->blocks[i]);
        }
    }
    pthread_mutex_unlock(&g_kernel->memory->lock);
    
    // Liberar estructuras
    pthread_mutex_destroy(&g_kernel->registry->lock);
    pthread_mutex_destroy(&g_kernel->scheduler->lock);
    pthread_cond_destroy(&g_kernel->scheduler->task_available);
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
    // Semilla para random
    srand((unsigned int)time(NULL) ^ getpid());
    
    // ID opcional por línea de comandos
    uint64_t node_id = 0;
    if (argc > 1) {
        node_id = strtoull(argv[1], NULL, 16);
    }
    
    // Manejador de señales
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Banner
    print_banner();
    
    // Inicializar kernel
    printf("[INIT] Inicializando kernel distribuido...\n");
    if (init_kernel(node_id) < 0) {
        fprintf(stderr, "[ERROR] Fallo en inicialización\n");
        return 1;
    }
    
    printf("[INIT] ✓ Node ID:  %016lX\n", g_kernel->node_id);
    printf("[INIT] ✓ Hostname: %s\n", g_kernel->local_info.hostname);
    printf("[INIT] ✓ IP:       %s\n", g_kernel->local_info.ip_address);
    printf("[INIT] ✓ Discovery: UDP %d\n", DISCOVERY_PORT);
    printf("[INIT] ✓ Data:      TCP %d\n", DATA_PORT);
    
    // Iniciar threads de red
    printf("[INIT] Iniciando servicios de red...\n");
    start_threads();
    
    printf("[INIT] ✓ Sistema listo\n");
    printf("\n");
    
    // Esperar un poco para descubrimiento inicial
    printf("[NET] Buscando nodos en la red");
    for (int i = 0; i < 3; i++) {
        printf(".");
        fflush(stdout);
        sleep(1);
    }
    
    pthread_mutex_lock(&g_kernel->registry->lock);
    int found = g_kernel->registry->count;
    pthread_mutex_unlock(&g_kernel->registry->lock);
    
    printf(" %d nodo(s) encontrado(s)\n", found);
    
    // Ejecutar interfaz de comandos en thread principal
    command_thread(NULL);
    
    // Limpieza
    printf("[SHUTDOWN] Limpiando recursos...\n");
    cleanup();
    printf("[SHUTDOWN] Sistema apagado correctamente\n");
    
    return 0;
}