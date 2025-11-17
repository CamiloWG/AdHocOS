#ifndef NETWORK_H
#define NETWORK_H

#include "../common.h"

// ========================================
// ESTRUCTURAS DE RED
// ========================================

typedef struct {
    int node_id;
    int port;
    Node nodes[MAX_NODES];
    int node_count;
    pthread_t listener_thread;
    int running;
    int messages_sent;
    int messages_received;
} NetworkManager;

// ========================================
// FUNCIONES PÚBLICAS
// ========================================

// Gestión del network manager
NetworkManager* create_network_manager(int node_id, int port);
void start_network_manager(NetworkManager* nm);
void stop_network_manager(NetworkManager* nm);
void destroy_network_manager(NetworkManager* nm);

// Envío de mensajes
int send_message(Node* dest_node, Message* msg);
int broadcast_message(Node nodes[], int node_count, Message* msg, int exclude_node);
void send_heartbeat(NetworkManager* nm);

// Procesamiento de mensajes
void* network_listener_thread(void* arg);
void process_received_message(NetworkManager* nm, Message* msg);
void handle_heartbeat(NetworkManager* nm, Message* msg);
void handle_discovery(NetworkManager* nm, Message* msg);

#endif // NETWORK_H