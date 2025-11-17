#include "../common.h"
#include "network.h"
#include <sys/select.h>
#include <fcntl.h>

// ========================================
// COMUNICACIÓN DE RED
// ========================================

int send_message(Node* dest_node, Message* msg) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_error("Error creando socket: %s", strerror(errno));
        return -1;
    }
    
    // Timeout para connect
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dest_node->port);
    server_addr.sin_addr.s_addr = inet_addr(dest_node->ip_address);
    
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_debug("No se pudo conectar a nodo %d: %s", 
                  dest_node->node_id, strerror(errno));
        close(sockfd);
        return -1;
    }
    
    msg->timestamp = time(NULL);
    ssize_t sent = send(sockfd, msg, sizeof(Message), 0);
    close(sockfd);
    
    if (sent < 0) {
        log_error("Error enviando mensaje: %s", strerror(errno));
        return -1;
    }
    
    log_debug("Mensaje tipo %d enviado a nodo %d", msg->type, dest_node->node_id);
    return 0;
}

int broadcast_message(Node nodes[], int node_count, Message* msg, int exclude_node) {
    int success_count = 0;
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].node_id != exclude_node && 
            nodes[i].status != NODE_OFFLINE && 
            nodes[i].status != NODE_FAILED) {
            
            if (send_message(&nodes[i], msg) == 0) {
                success_count++;
            }
        }
    }
    
    log_debug("Broadcast enviado a %d/%d nodos", success_count, node_count);
    return success_count;
}

void* network_listener_thread(void* arg) {
    NetworkManager* nm = (NetworkManager*)arg;
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_error("Error creando socket servidor");
        return NULL;
    }
    
    // Permitir reutilización de puerto
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(nm->port);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Error en bind: %s", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    if (listen(server_fd, 10) < 0) {
        log_error("Error en listen: %s", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    log_info("Listener de red iniciado en puerto %d", nm->port);
    
    // Set non-blocking
    fcntl(server_fd, F_SETFL, O_NONBLOCK);
    
    while (nm->running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            log_error("Error en select: %s", strerror(errno));
            break;
        }
        
        if (activity > 0 && FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0) {
                Message msg;
                ssize_t received = recv(client_fd, &msg, sizeof(Message), 0);
                
                if (received > 0) {
                    nm->messages_received++;
                    process_received_message(nm, &msg);
                }
                
                close(client_fd);
            }
        }
    }
    
    close(server_fd);
    log_info("Listener de red detenido");
    return NULL;
}

void process_received_message(NetworkManager* nm, Message* msg) {
    log_debug("Mensaje recibido: tipo=%d, origen=%d", msg->type, msg->source_node);
    
    switch(msg->type) {
        case MSG_HEARTBEAT:
            handle_heartbeat(nm, msg);
            break;
            
        case MSG_TASK:
            log_info("📥 Tarea recibida del nodo %d", msg->source_node);
            break;
            
        case MSG_DATA:
            log_debug("Datos recibidos: %d bytes", msg->data_size);
            break;
            
        case MSG_SYNC:
            log_debug("Mensaje de sincronización recibido");
            break;
            
        case MSG_DISCOVERY:
            handle_discovery(nm, msg);
            break;
            
        case MSG_LOCK_REQUEST:
        case MSG_LOCK_RELEASE:
            log_debug("Mensaje de lock recibido");
            break;
            
        default:
            log_error("Tipo de mensaje desconocido: %d", msg->type);
    }
}

void handle_heartbeat(NetworkManager* nm, Message* msg) {
    // Actualizar información del nodo
    for (int i = 0; i < nm->node_count; i++) {
        if (nm->nodes[i].node_id == msg->source_node) {
            nm->nodes[i].last_heartbeat = time(NULL);
            if (nm->nodes[i].status == NODE_FAILED || nm->nodes[i].status == NODE_OFFLINE) {
                log_info("♻️  Nodo %d recuperado", msg->source_node);
                nm->nodes[i].status = NODE_IDLE;
            }
            break;
        }
    }
}

void handle_discovery(NetworkManager* nm, Message* msg) {
    log_info("🔍 Nodo %d descubierto en la red", msg->source_node);
    
    // Verificar si ya conocemos este nodo
    int found = 0;
    for (int i = 0; i < nm->node_count; i++) {
        if (nm->nodes[i].node_id == msg->source_node) {
            found = 1;
            break;
        }
    }
    
    if (!found && nm->node_count < MAX_NODES) {
        // Agregar nuevo nodo
        Node new_node;
        memcpy(&new_node, msg->data, sizeof(Node));
        nm->nodes[nm->node_count++] = new_node;
        log_info("Nuevo nodo agregado: ID=%d", new_node.node_id);
    }
}

void send_heartbeat(NetworkManager* nm) {
    Message msg;
    msg.type = MSG_HEARTBEAT;
    msg.source_node = nm->node_id;
    msg.dest_node = -1; // Broadcast
    msg.data_size = 0;
    
    broadcast_message(nm->nodes, nm->node_count, &msg, nm->node_id);
    log_debug("💓 Heartbeat enviado");
}

NetworkManager* create_network_manager(int node_id, int port) {
    NetworkManager* nm = (NetworkManager*)malloc(sizeof(NetworkManager));
    nm->node_id = node_id;
    nm->port = port;
    nm->running = 0;
    nm->node_count = 0;
    nm->messages_sent = 0;
    nm->messages_received = 0;
    
    log_info("Gestor de red creado para nodo %d en puerto %d", node_id, port);
    return nm;
}

void start_network_manager(NetworkManager* nm) {
    nm->running = 1;
    pthread_create(&nm->listener_thread, NULL, network_listener_thread, nm);
    log_info("Gestor de red iniciado");
}

void stop_network_manager(NetworkManager* nm) {
    if (nm->running) {
        nm->running = 0;
        pthread_join(nm->listener_thread, NULL);
        log_info("Gestor de red detenido");
    }
}

void destroy_network_manager(NetworkManager* nm) {
    if (nm) {
        stop_network_manager(nm);
        free(nm);
    }
}