#include "../common.h"
#include "network.h"
#include "discovery.h"

// ========================================
// DESCUBRIMIENTO DE NODOS (Ad hoc)
// ========================================

void* discovery_thread(void* arg) {
    DiscoveryManager* dm = (DiscoveryManager*)arg;
    
    log_info("🔍 Iniciando descubrimiento de nodos...");
    
    // Crear socket UDP para broadcast
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log_error("Error creando socket de descubrimiento");
        return NULL;
    }
    
    // Habilitar broadcast
    int broadcast_enable = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
    
    struct sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    
    while (dm->running) {
        // Enviar beacon de descubrimiento
        Message discovery_msg;
        discovery_msg.type = MSG_DISCOVERY;
        discovery_msg.source_node = dm->node_id;
        discovery_msg.dest_node = -1; // Broadcast
        discovery_msg.timestamp = time(NULL);
        
        // Incluir información del nodo
        Node self_info;
        self_info.node_id = dm->node_id;
        sprintf(self_info.ip_address, "127.0.0.1"); // Simplificado para pruebas
        self_info.port = NODE_PORT_BASE + dm->node_id;
        self_info.status = NODE_IDLE;
        self_info.cpu_load = 0.3;
        self_info.memory_usage = 0.4;
        self_info.reputation = 0.9;
        self_info.last_heartbeat = time(NULL);
        
        memcpy(discovery_msg.data, &self_info, sizeof(Node));
        discovery_msg.data_size = sizeof(Node);
        
        sendto(sockfd, &discovery_msg, sizeof(Message), 0,
               (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        
        log_debug("Beacon de descubrimiento enviado");
        
        sleep(10); // Enviar cada 10 segundos
    }
    
    close(sockfd);
    return NULL;
}

void* discovery_listener_thread(void* arg) {
    DiscoveryManager* dm = (DiscoveryManager*)arg;
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log_error("Error creando socket listener de descubrimiento");
        return NULL;
    }
    
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DISCOVERY_PORT);
    
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Error en bind de descubrimiento: %s", strerror(errno));
        close(sockfd);
        return NULL;
    }
    
    log_info("Listener de descubrimiento activo");
    
    // Timeout para recvfrom
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    while (dm->running) {
        Message msg;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        ssize_t received = recvfrom(sockfd, &msg, sizeof(Message), 0,
                                     (struct sockaddr*)&client_addr, &client_len);
        
        if (received > 0 && msg.type == MSG_DISCOVERY) {
            if (msg.source_node != dm->node_id) {
                Node discovered_node;
                memcpy(&discovered_node, msg.data, sizeof(Node));
                add_discovered_node(dm, &discovered_node);
            }
        }
    }
    
    close(sockfd);
    return NULL;
}

void add_discovered_node(DiscoveryManager* dm, Node* node) {
    pthread_mutex_lock(&dm->lock);
    
    // Verificar si ya existe
    int found = 0;
    for (int i = 0; i < dm->discovered_count; i++) {
        if (dm->discovered_nodes[i].node_id == node->node_id) {
            // Actualizar información
            dm->discovered_nodes[i] = *node;
            found = 1;
            break;
        }
    }
    
    if (!found && dm->discovered_count < MAX_NODES) {
        dm->discovered_nodes[dm->discovered_count++] = *node;
        log_info("✨ Nuevo nodo descubierto: ID=%d, IP=%s:%d", 
                 node->node_id, node->ip_address, node->port);
    }
    
    pthread_mutex_unlock(&dm->lock);
}

DiscoveryManager* create_discovery_manager(int node_id) {
    DiscoveryManager* dm = (DiscoveryManager*)malloc(sizeof(DiscoveryManager));
    dm->node_id = node_id;
    dm->discovered_count = 0;
    dm->running = 0;
    pthread_mutex_init(&dm->lock, NULL);
    
    log_info("Gestor de descubrimiento creado");
    return dm;
}

void start_discovery(DiscoveryManager* dm) {
    dm->running = 1;
    pthread_create(&dm->discovery_thread, NULL, discovery_thread, dm);
    pthread_create(&dm->listener_thread, NULL, discovery_listener_thread, dm);
    log_info("Descubrimiento de nodos iniciado");
}

void stop_discovery(DiscoveryManager* dm) {
    if (dm->running) {
        dm->running = 0;
        pthread_join(dm->discovery_thread, NULL);
        pthread_join(dm->listener_thread, NULL);
        log_info("Descubrimiento de nodos detenido");
    }
}

void destroy_discovery_manager(DiscoveryManager* dm) {
    if (dm) {
        stop_discovery(dm);
        pthread_mutex_destroy(&dm->lock);
        free(dm);
    }
}

void simulate_node_discovery(DiscoveryManager* dm, int total_nodes) {
    log_info("🔧 Simulando descubrimiento de %d nodos...", total_nodes);
    
    for (int i = 0; i < total_nodes; i++) {
        if (i != dm->node_id) {
            Node simulated_node;
            simulated_node.node_id = i;
            sprintf(simulated_node.ip_address, "127.0.0.1");
            simulated_node.port = NODE_PORT_BASE + i;
            simulated_node.status = NODE_IDLE;
            simulated_node.cpu_load = 0.2 + (i * 0.05);
            simulated_node.memory_usage = 0.3 + (i * 0.05);
            simulated_node.reputation = 0.85 + (i * 0.02);
            simulated_node.last_heartbeat = time(NULL);
            simulated_node.task_count = 0;
            
            add_discovered_node(dm, &simulated_node);
        }
    }
}