#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "../common.h"

// ========================================
// GESTOR DE DESCUBRIMIENTO
// ========================================

typedef struct {
    int node_id;
    Node discovered_nodes[MAX_NODES];
    int discovered_count;
    pthread_t discovery_thread;
    pthread_t listener_thread;
    pthread_mutex_t lock;
    int running;
} DiscoveryManager;

// ========================================
// FUNCIONES PÚBLICAS
// ========================================

DiscoveryManager* create_discovery_manager(int node_id);
void start_discovery(DiscoveryManager* dm);
void stop_discovery(DiscoveryManager* dm);
void destroy_discovery_manager(DiscoveryManager* dm);

void* discovery_thread(void* arg);
void* discovery_listener_thread(void* arg);
void add_discovered_node(DiscoveryManager* dm, Node* node);

// Para pruebas
void simulate_node_discovery(DiscoveryManager* dm, int total_nodes);

#endif // DISCOVERY_H