// network_discovery.c - Sistema REAL de descubrimiento de nodos
// Usa broadcast UDP para descubrir nodos en la red local

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define DISCOVERY_PORT 8888
#define DATA_PORT 8889
#define BROADCAST_INTERVAL 5  // segundos
#define NODE_TIMEOUT 15       // segundos para considerar nodo muerto
#define MAX_NODES 100
#define BUFFER_SIZE 4096

// ========================================
// ESTRUCTURAS DE RED REAL
// ========================================

typedef struct {
    uint32_t magic;           // 0xDEADBEEF para identificar nuestro protocolo
    uint32_t version;         // Versión del protocolo
    uint32_t msg_type;        // Tipo de mensaje
    uint64_t node_id;         // ID único del nodo
    uint32_t sequence;        // Número de secuencia
    uint32_t payload_size;    // Tamaño del payload
} MessageHeader;

// Tipos de mensajes
enum MessageType {
    MSG_DISCOVERY_REQUEST = 1,
    MSG_DISCOVERY_RESPONSE,
    MSG_HEARTBEAT,
    MSG_NODE_INFO,
    MSG_TASK_REQUEST,
    MSG_TASK_RESPONSE,
    MSG_DATA_SYNC,
    MSG_NODE_LEAVE
};

// Información del nodo para discovery
typedef struct {
    uint64_t node_id;
    char hostname[256];
    char ip_address[INET_ADDRSTRLEN];
    uint16_t data_port;
    float cpu_load;
    float memory_usage;
    uint64_t capabilities;    // Flags de capacidades
    time_t timestamp;
} NodeInfo;

// Nodo en la red
typedef struct {
    NodeInfo info;
    time_t last_seen;
    int active;
    pthread_mutex_t lock;
} NetworkNode;

// Gestor de red
typedef struct {
    uint64_t local_node_id;
    NodeInfo local_info;
    NetworkNode nodes[MAX_NODES];
    int node_count;
    pthread_mutex_t nodes_lock;
    
    int discovery_socket;
    int data_socket;
    int running;
    
    pthread_t discovery_thread;
    pthread_t listener_thread;
    pthread_t heartbeat_thread;
} NetworkManager;

static NetworkManager* g_network = NULL;

// ========================================
// FUNCIONES DE UTILIDAD
// ========================================

// Generar ID único para el nodo
uint64_t generate_node_id() {
    // Combinar MAC address + timestamp para ID único
    struct ifaddrs *ifap, *ifa;
    uint64_t id = 0;
    
    if (getifaddrs(&ifap) == 0) {
        for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                if (!(ifa->ifa_flags & IFF_LOOPBACK)) {
                    // Usar la dirección IP como parte del ID
                    struct sockaddr_in *sa = (struct sockaddr_in*)ifa->ifa_addr;
                    id ^= (uint64_t)sa->sin_addr.s_addr;
                }
            }
        }
        freeifaddrs(ifap);
    }
    
    // Añadir timestamp para unicidad
    id ^= ((uint64_t)time(NULL) << 32);
    id ^= (uint64_t)getpid();
    
    return id;
}

// Obtener IP local (no loopback)
int get_local_ip(char* buffer, size_t buflen) {
    struct ifaddrs *ifap, *ifa;
    
    if (getifaddrs(&ifap) != 0) {
        return -1;
    }
    
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

// Obtener información del sistema
void get_system_info(NodeInfo* info) {
    // CPU Load
    FILE* fp = fopen("/proc/loadavg", "r");
    if (fp) {
        fscanf(fp, "%f", &info->cpu_load);
        fclose(fp);
    }
    
    // Memory usage
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
// SOCKET DE BROADCAST PARA DISCOVERY
// ========================================

int create_broadcast_socket() {
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
    
    // Permitir reusar dirección
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt reuseaddr");
        close(sock);
        return -1;
    }
    
    return sock;
}

int bind_discovery_socket(int sock) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DISCOVERY_PORT);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }
    
    return 0;
}

// ========================================
// ENVÍO Y RECEPCIÓN DE MENSAJES
// ========================================

void send_discovery_broadcast() {
    if (!g_network) return;
    
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);
    
    // Preparar mensaje de discovery
    char buffer[BUFFER_SIZE];
    MessageHeader* header = (MessageHeader*)buffer;
    NodeInfo* info = (NodeInfo*)(buffer + sizeof(MessageHeader));
    
    header->magic = htonl(0xDEADBEEF);
    header->version = htonl(1);
    header->msg_type = htonl(MSG_DISCOVERY_REQUEST);
    header->node_id = htobe64(g_network->local_node_id);
    header->sequence = htonl(time(NULL));
    header->payload_size = htonl(sizeof(NodeInfo));
    
    // Actualizar información local
    get_system_info(&g_network->local_info);
    memcpy(info, &g_network->local_info, sizeof(NodeInfo));
    
    int msg_size = sizeof(MessageHeader) + sizeof(NodeInfo);
    
    if (sendto(g_network->discovery_socket, buffer, msg_size, 0,
               (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("sendto broadcast");
    } else {
        printf("[DISCOVERY] Broadcast enviado - Node ID: %016lX\n", 
               g_network->local_node_id);
    }
}

void process_discovery_message(char* buffer, ssize_t size, struct sockaddr_in* sender) {
    if (size < sizeof(MessageHeader)) return;
    
    MessageHeader* header = (MessageHeader*)buffer;
    
    // Verificar magic number
    if (ntohl(header->magic) != 0xDEADBEEF) {
        return; // No es nuestro protocolo
    }
    
    uint64_t remote_node_id = be64toh(header->node_id);
    
    // Ignorar nuestros propios mensajes
    if (remote_node_id == g_network->local_node_id) {
        return;
    }
    
    uint32_t msg_type = ntohl(header->msg_type);
    
    if (msg_type == MSG_DISCOVERY_REQUEST || msg_type == MSG_DISCOVERY_RESPONSE) {
        if (size < sizeof(MessageHeader) + sizeof(NodeInfo)) return;
        
        NodeInfo* info = (NodeInfo*)(buffer + sizeof(MessageHeader));
        
        // Actualizar o añadir nodo
        pthread_mutex_lock(&g_network->nodes_lock);
        
        int found = -1;
        for (int i = 0; i < g_network->node_count; i++) {
            if (g_network->nodes[i].info.node_id == remote_node_id) {
                found = i;
                break;
            }
        }
        
        if (found >= 0) {
            // Actualizar nodo existente
            memcpy(&g_network->nodes[found].info, info, sizeof(NodeInfo));
            g_network->nodes[found].last_seen = time(NULL);
            g_network->nodes[found].active = 1;
            
            printf("[DISCOVERY] Nodo actualizado: %016lX desde %s\n", 
                   remote_node_id, inet_ntoa(sender->sin_addr));
        } else if (g_network->node_count < MAX_NODES) {
            // Añadir nuevo nodo
            NetworkNode* node = &g_network->nodes[g_network->node_count];
            memcpy(&node->info, info, sizeof(NodeInfo));
            strcpy(node->info.ip_address, inet_ntoa(sender->sin_addr));
            node->last_seen = time(NULL);
            node->active = 1;
            pthread_mutex_init(&node->lock, NULL);
            
            g_network->node_count++;
            
            printf("[DISCOVERY] Nuevo nodo descubierto: %016lX\n", remote_node_id);
            printf("  Hostname: %s\n", info->hostname);
            printf("  IP: %s\n", node->info.ip_address);
            printf("  CPU Load: %.2f%%\n", info->cpu_load * 100);
            printf("  Memory: %.2f%%\n", info->memory_usage * 100);
        }
        
        pthread_mutex_unlock(&g_network->nodes_lock);
        
        // Si recibimos un REQUEST, enviar RESPONSE
        if (msg_type == MSG_DISCOVERY_REQUEST) {
            // Enviar respuesta unicast
            header->msg_type = htonl(MSG_DISCOVERY_RESPONSE);
            header->node_id = htobe64(g_network->local_node_id);
            
            NodeInfo* response_info = (NodeInfo*)(buffer + sizeof(MessageHeader));
            memcpy(response_info, &g_network->local_info, sizeof(NodeInfo));
            
            sendto(g_network->discovery_socket, buffer, 
                   sizeof(MessageHeader) + sizeof(NodeInfo), 0,
                   (struct sockaddr*)sender, sizeof(*sender));
        }
    }
}

// ========================================
// THREADS DE RED
// ========================================

void* discovery_thread(void* arg) {
    (void)arg;
    
    while (g_network->running) {
        send_discovery_broadcast();
        sleep(BROADCAST_INTERVAL);
    }
    
    return NULL;
}

void* listener_thread(void* arg) {
    (void)arg;
    
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    
    while (g_network->running) {
        ssize_t size = recvfrom(g_network->discovery_socket, buffer, BUFFER_SIZE, 0,
                               (struct sockaddr*)&sender, &sender_len);
        
        if (size > 0) {
            process_discovery_message(buffer, size, &sender);
        }
    }
    
    return NULL;
}

void* heartbeat_thread(void* arg) {
    (void)arg;
    
    while (g_network->running) {
        time_t current = time(NULL);
        
        pthread_mutex_lock(&g_network->nodes_lock);
        
        for (int i = 0; i < g_network->node_count; i++) {
            if (current - g_network->nodes[i].last_seen > NODE_TIMEOUT) {
                if (g_network->nodes[i].active) {
                    g_network->nodes[i].active = 0;
                    printf("[HEARTBEAT] Nodo %016lX timeout\n", 
                           g_network->nodes[i].info.node_id);
                }
            }
        }
        
        pthread_mutex_unlock(&g_network->nodes_lock);
        
        sleep(5);
    }
    
    return NULL;
}

// ========================================
// API PÚBLICA
// ========================================

int init_network_discovery(uint64_t node_id) {
    g_network = calloc(1, sizeof(NetworkManager));
    if (!g_network) return -1;
    
    g_network->local_node_id = node_id ? node_id : generate_node_id();
    g_network->running = 1;
    
    // Configurar información local
    gethostname(g_network->local_info.hostname, sizeof(g_network->local_info.hostname));
    get_local_ip(g_network->local_info.ip_address, sizeof(g_network->local_info.ip_address));
    g_network->local_info.node_id = g_network->local_node_id;
    g_network->local_info.data_port = DATA_PORT;
    get_system_info(&g_network->local_info);
    
    // Crear socket de discovery
    g_network->discovery_socket = create_broadcast_socket();
    if (g_network->discovery_socket < 0) {
        free(g_network);
        return -1;
    }
    
    if (bind_discovery_socket(g_network->discovery_socket) < 0) {
        close(g_network->discovery_socket);
        free(g_network);
        return -1;
    }
    
    pthread_mutex_init(&g_network->nodes_lock, NULL);
    
    // Iniciar threads
    pthread_create(&g_network->discovery_thread, NULL, discovery_thread, NULL);
    pthread_create(&g_network->listener_thread, NULL, listener_thread, NULL);
    pthread_create(&g_network->heartbeat_thread, NULL, heartbeat_thread, NULL);
    
    printf("[NETWORK] Sistema de red inicializado\n");
    printf("  Node ID: %016lX\n", g_network->local_node_id);
    printf("  Hostname: %s\n", g_network->local_info.hostname);
    printf("  IP: %s\n", g_network->local_info.ip_address);
    printf("  Discovery Port: %d\n", DISCOVERY_PORT);
    
    return 0;
}

void shutdown_network_discovery() {
    if (!g_network) return;
    
    g_network->running = 0;
    
    // Enviar mensaje de salida
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);
    
    MessageHeader header;
    header.magic = htonl(0xDEADBEEF);
    header.version = htonl(1);
    header.msg_type = htonl(MSG_NODE_LEAVE);
    header.node_id = htobe64(g_network->local_node_id);
    header.sequence = htonl(time(NULL));
    header.payload_size = 0;
    
    sendto(g_network->discovery_socket, &header, sizeof(header), 0,
           (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    
    // Esperar threads
    pthread_join(g_network->discovery_thread, NULL);
    pthread_join(g_network->listener_thread, NULL);
    pthread_join(g_network->heartbeat_thread, NULL);
    
    close(g_network->discovery_socket);
    pthread_mutex_destroy(&g_network->nodes_lock);
    
    free(g_network);
    g_network = NULL;
}

int get_active_nodes(NetworkNode** nodes) {
    if (!g_network) return 0;
    
    pthread_mutex_lock(&g_network->nodes_lock);
    
    int count = 0;
    for (int i = 0; i < g_network->node_count; i++) {
        if (g_network->nodes[i].active) {
            count++;
        }
    }
    
    if (nodes) {
        *nodes = g_network->nodes;
    }
    
    pthread_mutex_unlock(&g_network->nodes_lock);
    
    return count;
}

void print_network_status() {
    if (!g_network) return;
    
    pthread_mutex_lock(&g_network->nodes_lock);
    
    printf("\n=== Estado de la Red ===\n");
    printf("Nodo Local: %016lX (%s)\n", 
           g_network->local_node_id, g_network->local_info.hostname);
    printf("Nodos Activos: %d\n\n", g_network->node_count);
    
    for (int i = 0; i < g_network->node_count; i++) {
        if (g_network->nodes[i].active) {
            NodeInfo* info = &g_network->nodes[i].info;
            printf("Nodo %d:\n", i + 1);
            printf("  ID: %016lX\n", info->node_id);
            printf("  Host: %s\n", info->hostname);
            printf("  IP: %s:%d\n", info->ip_address, info->data_port);
            printf("  CPU: %.1f%%, Mem: %.1f%%\n", 
                   info->cpu_load * 100, info->memory_usage * 100);
            printf("  Última vez visto: hace %ld segundos\n", 
                   time(NULL) - g_network->nodes[i].last_seen);
        }
    }
    
    pthread_mutex_unlock(&g_network->nodes_lock);
}

// ========================================
// COMUNICACIÓN DE DATOS ENTRE NODOS
// ========================================

int send_data_to_node(uint64_t node_id, void* data, size_t size) {
    if (!g_network) return -1;
    
    pthread_mutex_lock(&g_network->nodes_lock);
    
    NetworkNode* target = NULL;
    for (int i = 0; i < g_network->node_count; i++) {
        if (g_network->nodes[i].info.node_id == node_id && g_network->nodes[i].active) {
            target = &g_network->nodes[i];
            break;
        }
    }
    
    if (!target) {
        pthread_mutex_unlock(&g_network->nodes_lock);
        return -1;
    }
    
    // Crear socket TCP para envío de datos
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        pthread_mutex_unlock(&g_network->nodes_lock);
        return -1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(target->info.data_port);
    inet_pton(AF_INET, target->info.ip_address, &addr.sin_addr);
    
    pthread_mutex_unlock(&g_network->nodes_lock);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    // Enviar header + datos
    MessageHeader header;
    header.magic = htonl(0xDEADBEEF);
    header.version = htonl(1);
    header.msg_type = htonl(MSG_DATA_SYNC);
    header.node_id = htobe64(g_network->local_node_id);
    header.sequence = htonl(time(NULL));
    header.payload_size = htonl(size);
    
    send(sock, &header, sizeof(header), 0);
    send(sock, data, size, 0);
    
    close(sock);
    return 0;
}