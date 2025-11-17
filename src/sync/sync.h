#ifndef SYNC_H
#define SYNC_H

#include "../common.h"

// ========================================
// ESTRUCTURAS DE SINCRONIZACIÓN
// ========================================

// Mutex distribuido (Algoritmo de Lamport)
typedef struct {
    int timestamp;
    int node_id;
    int requesting;
    int granted;
    int replies_received;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} LamportMutex;

// Barrera distribuida
typedef struct {
    int node_id;
    int total_nodes;
    int arrived_count;
    int arrived_nodes[MAX_NODES];
    int generation;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} DistributedBarrier;

// Reloj lógico de Lamport
typedef struct {
    int timestamp;
    int node_id;
    pthread_mutex_t lock;
} LogicalClock;

// ========================================
// FUNCIONES PÚBLICAS
// ========================================

// Mutex de Lamport
LamportMutex* create_lamport_mutex(int node_id);
void acquire_distributed_lock(LamportMutex* mutex, int total_nodes);
void release_distributed_lock(LamportMutex* mutex);
void handle_lock_request(LamportMutex* mutex, int requesting_node, int request_timestamp);
void handle_lock_reply(LamportMutex* mutex);
void destroy_lamport_mutex(LamportMutex* mutex);

// Barreras
DistributedBarrier* create_distributed_barrier(int node_id, int total_nodes);
void wait_at_barrier(DistributedBarrier* barrier);
void destroy_distributed_barrier(DistributedBarrier* barrier);

// Reloj lógico
LogicalClock* create_logical_clock(int node_id);
int increment_clock(LogicalClock* clock);
int update_clock(LogicalClock* clock, int received_timestamp);
int get_clock_time(LogicalClock* clock);
void destroy_logical_clock(LogicalClock* clock);

#endif // SYNC_H