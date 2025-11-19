// ========================================
// SISTEMA OPERATIVO DESCENTRALIZADO - MAIN COMPLETO
// Integración de todos los módulos sobre Alpine Linux
// ========================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <errno.h>

// ========================================
// CONSTANTES Y CONFIGURACIÓN
// ========================================

#define MAX_NODES 100
#define MAX_TASKS 1000
#define DISCOVERY_PORT 8888
#define DATA_PORT 8889
#define BROADCAST_INTERVAL 5
#define NODE_TIMEOUT 15
#define BUFFER_SIZE 4096

// ========================================
// ESTRUCTURAS DE RED
// ========================================

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t msg_type;
    uint64_t node_id;
    uint32_t sequence;
    uint32_t payload_size;
} __attribute__((packed)) MessageHeader;

typedef enum {
    MSG_DISCOVERY_REQUEST = 1,
    MSG_DISCOVERY_RESPONSE,
    MSG_HEARTBEAT,
    MSG_NODE_INFO,
    MSG_TASK_REQUEST,
    MSG_TASK_RESPONSE,
    MSG_DATA_SYNC,
    MSG_NODE_LEAVE
} MessageType;

typedef struct {
    uint64_t node_id;
    char hostname[256];
    char ip_address[16];
    uint16_t data_port;
    float cpu_load;
    float memory_usage;
    uint64_t capabilities;
    time_t timestamp;
} NodeInfo;

typedef struct {
    NodeInfo info;
    time_t last_seen;
    bool active;
    pthread_mutex_t lock;
} NetworkNode;

typedef struct {
    uint64_t local_node_id;
    NodeInfo local_info;
    NetworkNode nodes[MAX_NODES];
    int node_count;
    pthread_mutex_t nodes_lock;
    int discovery_socket;
    bool running;
    pthread_t discovery_thread;
    pthread_t listener_thread;
    pthread_t heartbeat_thread;
} NetworkManager;

// ========================================
// ESTRUCTURAS DEL SISTEMA
// ========================================

typedef struct {
    uint64_t task_id;
    char description[256];
    int priority;
    uint64_t assigned_node;
    void* (*task_function)(void*);
    void* task_data;
    size_t data_size;
    int status;
    time_t creation_time;
} Task;

typedef struct {
    Task* tasks;
    size_t capacity;
    size_t count;
    pthread_mutex_t lock;
} TaskScheduler;

typedef struct {
    uint64_t memory_id;
    void* data;
    size_t size;
    uint64_t owner_node;
    int reference_count;
    pthread_rwlock_t rwlock;
} SharedMemory;

typedef struct {
    SharedMemory** blocks;
    size_t capacity;
    size_t count;
    pthread_rwlock_t lock;
} MemoryManager;

typedef struct {
    uint64_t node_id;
    NetworkManager* network;
    TaskScheduler* scheduler;
    MemoryManager* memory;
    bool running;
    pthread_t command_thread;
    pthread_t server_thread;
} DecentralizedKernel;

// ========================================
// VARIABLES GLOBALES
// ========================================

static DecentralizedKernel* g_kernel = NULL;

// ========================================
// FUNCIONES DE UTILIDAD
// ========================================

uint64_t generate_node_id() {
    struct ifaddrs *ifap, *ifa;
    uint64_t id = 0;
    
    if (getifaddrs(&ifap) == 0) {
        for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                if (!(ifa->ifa_flags & IFF_LOOPBACK)) {
                    struct sockaddr_in *sa = (struct sockaddr_in*)ifa->ifa_addr;
                    id ^= (uint64_t)sa->sin_addr.s_addr;
                }
            }
        }
        freeifaddrs(ifap);
    }
    
    id ^= ((uint64_t)time(NULL) << 32);
    id ^= (uint64_t)getpid();
    return id;
}

int get_local_ip(char* buffer, size_t buflen) {
    struct ifaddrs *ifap, *ifa;
    
    if (getifaddrs(&ifap) != 0) return -1;
    
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            if (!(ifa->ifa_flags & IFF_LOOPBACK) && (ifa->ifa_flags & IFF_UP)) {
                struct sockaddr_in *sa = (struct sockaddr_in*)ifa->ifa_addr;
                inet_ntop(AF_INET, &sa->sin_addr, buffer, buflen);
                freeifaddrs(ifap);
                return 0;
            }
        }
    }
    
    freeifaddrs(ifap);
    strcpy(buffer, "127.0.0.1");
    return -1;
}

void get_system_info(NodeInfo* info) {
    FILE* fp = fopen("/proc/loadavg", "r");
    if (fp) {
        fscanf(fp, "%f", &info->cpu_load);
        fclose(fp);
    }
    
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        long total = 0, available = 0;
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "MemTotal: %ld kB", &total) == 1) continue;
            if (sscanf(line, "MemAvailable: %ld kB", &available) == 1) break;
        }
        fclose(fp);
        if (total > 0) {
            info->memory_usage = 1.0 - ((float)available / total);
        }
    }
}

// ========================================
// FUNCIONES DE RED
// ========================================

int create_broadcast_socket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DISCOVERY_PORT);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

void send_discovery_broadcast(NetworkManager* nm) {
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);
    
    char buffer[BUFFER_SIZE];
    MessageHeader* header = (MessageHeader*)buffer;
    NodeInfo* info = (NodeInfo*)(buffer + sizeof(MessageHeader));
    
    header->magic = htonl(0xDEADBEEF);
    header->version = htonl(1);
    header->msg_type = htonl(MSG_DISCOVERY_REQUEST);
    header->node_id = htobe64(nm->local_node_id);
    header->sequence = htonl(time(NULL));
    header->payload_size = htonl(sizeof(NodeInfo));
    
    get_system_info(&nm->local_info);
    memcpy(info, &nm->local_info, sizeof(NodeInfo));
    
    sendto(nm->discovery_socket, buffer, sizeof(MessageHeader) + sizeof(NodeInfo), 0,
           (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    
    printf("[DISCOVERY] Broadcast enviado - Node ID: %016lX\n", nm->local_node_id);
}

void process_discovery_message(NetworkManager* nm, char* buffer, ssize_t size, 
                               struct sockaddr_in* sender) {
    if (size < sizeof(MessageHeader)) return;
    
    MessageHeader* header = (MessageHeader*)buffer;
    
    if (ntohl(header->magic) != 0xDEADBEEF) return;
    
    uint64_t remote_node_id = be64toh(header->node_id);
    if (remote_node_id == nm->local_node_id) return;
    
    uint32_t msg_type = ntohl(header->msg_type);
    
    if (msg_type == MSG_DISCOVERY_REQUEST || msg_type == MSG_DISCOVERY_RESPONSE) {
        if (size < sizeof(MessageHeader) + sizeof(NodeInfo)) return;
        
        NodeInfo* info = (NodeInfo*)(buffer + sizeof(MessageHeader));
        
        pthread_mutex_lock(&nm->nodes_lock);
        
        int found = -1;
        for (int i = 0; i < nm->node_count; i++) {
            if (nm->nodes[i].info.node_id == remote_node_id) {
                found = i;
                break;
            }
        }
        
        if (found >= 0) {
            memcpy(&nm->nodes[found].info, info, sizeof(NodeInfo));
            nm->nodes[found].last_seen = time(NULL);
            nm->nodes[found].active = true;
        } else if (nm->node_count < MAX_NODES) {
            NetworkNode* node = &nm->nodes[nm->node_count];
            memcpy(&node->info, info, sizeof(NodeInfo));
            strcpy(node->info.ip_address, inet_ntoa(sender->sin_addr));
            node->last_seen = time(NULL);
            node->active = true;
            pthread_mutex_init(&node->lock, NULL);
            nm->node_count++;
            
            printf("[DISCOVERY] ✨ Nuevo nodo: %016lX (%s)\n", 
                   remote_node_id, node->info.hostname);
        }
        
        pthread_mutex_unlock(&nm->nodes_lock);
        
        if (msg_type == MSG_DISCOVERY_REQUEST) {
            header->msg_type = htonl(MSG_DISCOVERY_RESPONSE);
            header->node_id = htobe64(nm->local_node_id);
            NodeInfo* response_info = (NodeInfo*)(buffer + sizeof(MessageHeader));
            memcpy(response_info, &nm->local_info, sizeof(NodeInfo));
            sendto(nm->discovery_socket, buffer, 
                   sizeof(MessageHeader) + sizeof(NodeInfo), 0,
                   (struct sockaddr*)sender, sizeof(*sender));
        }
    }
}

void* discovery_thread(void* arg) {
    NetworkManager* nm = (NetworkManager*)arg;
    while (nm->running) {
        send_discovery_broadcast(nm);
        sleep(BROADCAST_INTERVAL);
    }
    return NULL;
}

void* listener_thread(void* arg) {
    NetworkManager* nm = (NetworkManager*)arg;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    
    while (nm->running) {
        ssize_t size = recvfrom(nm->discovery_socket, buffer, BUFFER_SIZE, 0,
                               (struct sockaddr*)&sender, &sender_len);
        if (size > 0) {
            process_discovery_message(nm, buffer, size, &sender);
        }
    }
    return NULL;
}

void* heartbeat_thread(void* arg) {
    NetworkManager* nm = (NetworkManager*)arg;
    
    while (nm->running) {
        time_t current = time(NULL);
        pthread_mutex_lock(&nm->nodes_lock);
        
        for (int i = 0; i < nm->node_count; i++) {
            if (current - nm->nodes[i].last_seen > NODE_TIMEOUT) {
                if (nm->nodes[i].active) {
                    nm->nodes[i].active = false;
                    printf("[HEARTBEAT] ⚠️  Nodo %016lX timeout\n", 
                           nm->nodes[i].info.node_id);
                }
            }
        }
        
        pthread_mutex_unlock(&nm->nodes_lock);
        sleep(5);
    }
    return NULL;
}

// ========================================
// SCHEDULER DISTRIBUIDO
// ========================================

uint64_t find_best_node_for_task(Task* task, NetworkManager* nm) {
    pthread_mutex_lock(&nm->nodes_lock);
    
    if (nm->node_count == 0) {
        pthread_mutex_unlock(&nm->nodes_lock);
        return g_kernel->node_id;
    }
    
    uint64_t best_node = g_kernel->node_id;
    float best_score = 100.0;
    
    for (int i = 0; i < nm->node_count; i++) {
        if (nm->nodes[i].active) {
            float score = nm->nodes[i].info.cpu_load * 50 + 
                         nm->nodes[i].info.memory_usage * 50;
            
            if (score < best_score) {
                best_score = score;
                best_node = nm->nodes[i].info.node_id;
            }
        }
    }
    
    pthread_mutex_unlock(&nm->nodes_lock);
    return best_node;
}

int schedule_task(Task* task) {
    if (!g_kernel || !g_kernel->scheduler) return -1;
    
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    
    if (g_kernel->scheduler->count >= g_kernel->scheduler->capacity) {
        pthread_mutex_unlock(&g_kernel->scheduler->lock);
        return -1;
    }
    
    uint64_t best_node = find_best_node_for_task(task, g_kernel->network);
    task->assigned_node = best_node;
    task->status = 1;
    task->creation_time = time(NULL);
    
    g_kernel->scheduler->tasks[g_kernel->scheduler->count++] = *task;
    
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    
    printf("[SCHEDULER] ✅ Tarea %lu → Nodo %016lX\n", 
           task->task_id, best_node);
    
    return 0;
}

// ========================================
// INTERFAZ DE COMANDOS
// ========================================

void print_status() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║              ESTADO DEL SISTEMA                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("🖥️  Nodo Local:\n");
    printf("   ID: %016lX\n", g_kernel->node_id);
    printf("   Host: %s\n", g_kernel->network->local_info.hostname);
    printf("   IP: %s\n", g_kernel->network->local_info.ip_address);
    printf("   CPU: %.1f%% | RAM: %.1f%%\n", 
           g_kernel->network->local_info.cpu_load * 100,
           g_kernel->network->local_info.memory_usage * 100);
    printf("\n");
    
    pthread_mutex_lock(&g_kernel->network->nodes_lock);
    
    int active = 0;
    for (int i = 0; i < g_kernel->network->node_count; i++) {
        if (g_kernel->network->nodes[i].active) active++;
    }
    
    printf("🌐 Red Ad hoc:\n");
    printf("   Nodos activos: %d\n", active);
    printf("   Nodos totales: %d\n", g_kernel->network->node_count);
    printf("\n");
    
    if (active > 0) {
        printf("📋 Nodos conectados:\n");
        for (int i = 0; i < g_kernel->network->node_count; i++) {
            if (g_kernel->network->nodes[i].active) {
                NodeInfo* info = &g_kernel->network->nodes[i].info;
                printf("   • %016lX (%s)\n", info->node_id, info->hostname);
                printf("     IP: %s | CPU: %.0f%% | RAM: %.0f%%\n",
                       info->ip_address,
                       info->cpu_load * 100,
                       info->memory_usage * 100);
            }
        }
    }
    
    pthread_mutex_unlock(&g_kernel->network->nodes_lock);
    
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    printf("\n");
    printf("📊 Tareas:\n");
    printf("   Total: %zu\n", g_kernel->scheduler->count);
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    printf("\n");
}

void* command_thread(void* arg) {
    (void)arg;
    char command[256];
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║     Sistema Operativo Descentralizado - LISTO            ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Comandos disponibles:\n");
    printf("  status    - Ver estado completo del sistema\n");
    printf("  nodes     - Listar nodos activos\n");
    printf("  task <descripción> - Crear nueva tarea\n");
    printf("  tasks     - Ver todas las tareas\n");
    printf("  help      - Mostrar ayuda\n");
    printf("  exit      - Salir del sistema\n");
    printf("\n");
    
    while (g_kernel->running) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) break;
        command[strcspn(command, "\n")] = 0;
        
        if (strcmp(command, "status") == 0) {
            print_status();
        }
        else if (strcmp(command, "nodes") == 0) {
            pthread_mutex_lock(&g_kernel->network->nodes_lock);
            printf("\n📡 Nodos en la red:\n\n");
            printf("%-18s %-20s %-15s %-10s %-10s\n", 
                   "NODE ID", "HOSTNAME", "IP", "CPU", "RAM");
            printf("─────────────────────────────────────────────────────────────\n");
            
            for (int i = 0; i < g_kernel->network->node_count; i++) {
                if (g_kernel->network->nodes[i].active) {
                    NodeInfo* info = &g_kernel->network->nodes[i].info;
                    printf("%016lX  %-20s %-15s %.0f%%     %.0f%%\n",
                           info->node_id, info->hostname, info->ip_address,
                           info->cpu_load * 100, info->memory_usage * 100);
                }
            }
            pthread_mutex_unlock(&g_kernel->network->nodes_lock);
            printf("\n");
        }
        else if (strncmp(command, "task ", 5) == 0) {
            static uint64_t task_id = 0;
            Task task = {0};
            task.task_id = ++task_id;
            strncpy(task.description, command + 5, sizeof(task.description) - 1);
            task.priority = 5;
            schedule_task(&task);
        }
        else if (strcmp(command, "tasks") == 0) {
            pthread_mutex_lock(&g_kernel->scheduler->lock);
            printf("\n📋 Tareas del sistema:\n\n");
            printf("%-5s %-40s %-18s %-10s\n", "ID", "DESCRIPCIÓN", "NODO", "ESTADO");
            printf("────────────────────────────────────────────────────────────────────────\n");
            
            for (size_t i = 0; i < g_kernel->scheduler->count; i++) {
                Task* t = &g_kernel->scheduler->tasks[i];
                const char* status = t->status == 0 ? "Pendiente" :
                                    t->status == 1 ? "Ejecutando" :
                                    t->status == 2 ? "Completada" : "Fallida";
                printf("%-5lu %-40s %016lX  %-10s\n",
                       t->task_id, t->description, t->assigned_node, status);
            }
            pthread_mutex_unlock(&g_kernel->scheduler->lock);
            printf("\n");
        }
        else if (strcmp(command, "help") == 0) {
            printf("\n");
            printf("═══════════════════════════════════════════════════════════\n");
            printf("  AYUDA - Sistema Operativo Descentralizado\n");
            printf("═══════════════════════════════════════════════════════════\n");
            printf("\n");
            printf("COMANDOS:\n");
            printf("  status  - Muestra estado completo (nodos, tareas, recursos)\n");
            printf("  nodes   - Lista todos los nodos activos en la red\n");
            printf("  task    - Crea una tarea distribuida\n");
            printf("            Ejemplo: task Procesar dataset grande\n");
            printf("  tasks   - Lista todas las tareas del sistema\n");
            printf("  help    - Muestra esta ayuda\n");
            printf("  exit    - Sale del sistema\n");
            printf("\n");
            printf("RED AD HOC:\n");
            printf("  - Los nodos se descubren automáticamente\n");
            printf("  - Puerto UDP: 8888 (Discovery)\n");
            printf("  - Puerto TCP: 8889 (Datos)\n");
            printf("  - Broadcast cada 5 segundos\n");
            printf("\n");
        }
        else if (strcmp(command, "exit") == 0) {
            g_kernel->running = false;
            break;
        }
        else if (strlen(command) > 0) {
            printf("Comando desconocido: '%s'. Usa 'help' para ver comandos.\n", command);
        }
    }
    
    return NULL;
}

// ========================================
// MANEJADOR DE SEÑALES
// ========================================

void handle_signal(int sig) {
    (void)sig;
    printf("\n\n[SYSTEM] Apagando sistema...\n");
    if (g_kernel) {
        g_kernel->running = false;
    }
}

// ========================================
// INICIALIZACIÓN
// ========================================

int init_network(uint64_t node_id) {
    NetworkManager* nm = calloc(1, sizeof(NetworkManager));
    if (!nm) return -1;
    
    nm->local_node_id = node_id ? node_id : generate_node_id();
    nm->running = true;
    
    gethostname(nm->local_info.hostname, sizeof(nm->local_info.hostname));
    get_local_ip(nm->local_info.ip_address, sizeof(nm->local_info.ip_address));
    nm->local_info.node_id = nm->local_node_id;
    nm->local_info.data_port = DATA_PORT;
    get_system_info(&nm->local_info);
    
    nm->discovery_socket = create_broadcast_socket();
    if (nm->discovery_socket < 0) {
        free(nm);
        return -1;
    }
    
    pthread_mutex_init(&nm->nodes_lock, NULL);
    
    pthread_create(&nm->discovery_thread, NULL, discovery_thread, nm);
    pthread_create(&nm->listener_thread, NULL, listener_thread, nm);
    pthread_create(&nm->heartbeat_thread, NULL, heartbeat_thread, nm);
    
    printf("[NETWORK] ✅ Red inicializada\n");
    printf("  Node ID: %016lX\n", nm->local_node_id);
    printf("  Hostname: %s\n", nm->local_info.hostname);
    printf("  IP: %s\n", nm->local_info.ip_address);
    
    g_kernel->network = nm;
    return 0;
}

// ========================================
// MAIN
// ========================================

int main(int argc, char* argv[]) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                                                           ║\n");
    printf("║      SISTEMA OPERATIVO DESCENTRALIZADO v1.0              ║\n");
    printf("║      Para Redes Ad hoc                                   ║\n");
    printf("║                                                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    uint64_t node_id = 0;
    if (argc > 1) {
        node_id = strtoull(argv[1], NULL, 16);
    }
    
    g_kernel = calloc(1, sizeof(DecentralizedKernel));
    g_kernel->running = true;
    g_kernel->node_id = node_id;
    
    g_kernel->scheduler = calloc(1, sizeof(TaskScheduler));
    g_kernel->scheduler->capacity = MAX_TASKS;
    g_kernel->scheduler->tasks = calloc(MAX_TASKS, sizeof(Task));
    pthread_mutex_init(&g_kernel->scheduler->lock, NULL);
    
    g_kernel->memory = calloc(1, sizeof(MemoryManager));
    g_kernel->memory->capacity = 1000;
    g_kernel->memory->blocks = calloc(1000, sizeof(SharedMemory*));
    pthread_rwlock_init(&g_kernel->memory->lock, NULL);
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("[SYSTEM] Inicializando...\n");
    
    if (init_network(node_id) < 0) {
        fprintf(stderr, "[ERROR] No se pudo inicializar la red\n");
        return 1;
    }
    
    printf("\n[SYSTEM] 🔍 Descubriendo nodos (espera 10 segundos)...\n");
    sleep(10);
    
    pthread_create(&g_kernel->command_thread, NULL, command_thread, NULL);
    
    while (g_kernel->running) {
        sleep(1);
    }
    
    printf("\n[SYSTEM] Limpiando recursos...\n");
    
    if (g_kernel->network) {
        g_kernel->network->running = false;
        pthread_join(g_kernel->network->discovery_thread, NULL);
        pthread_join(g_kernel->network->listener_thread, NULL);
        pthread_join(g_kernel->network->heartbeat_thread, NULL);
        close(g_kernel->network->discovery_socket);
        free(g_kernel->network);
    }
    
    pthread_mutex_destroy(&g_kernel->scheduler->lock);
    free(g_kernel->scheduler->tasks);
    free(g_kernel->scheduler);
    
    pthread_rwlock_destroy(&g_kernel->memory->lock);
    free(g_kernel->memory->blocks);
    free(g_kernel->memory);
    
    free(g_kernel);
    
    printf("[SYSTEM] ✅ Apagado completo\n\n");
    
    return 0;
}