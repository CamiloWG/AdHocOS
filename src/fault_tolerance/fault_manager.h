#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include "../common.h"

// ========================================
// ESTRUCTURAS DE TOLERANCIA A FALLOS
// ========================================

typedef struct {
    char name[64];
    time_t timestamp;
    Node node_states[MAX_NODES];
    int node_count;
} Checkpoint;

typedef struct {
    Node* nodes;
    int node_count;
    pthread_t monitor_thread;
    pthread_mutex_t lock;
    int running;
    int failed_nodes;
    int recoveries;
    int tasks_recovered;
    Checkpoint checkpoints[10];
    int checkpoint_count;
} FaultToleranceManager;

// ========================================
// FUNCIONES PÚBLICAS
// ========================================

// Gestión del FT manager
FaultToleranceManager* create_fault_tolerance_manager();
void start_fault_tolerance(FaultToleranceManager* ftm);
void stop_fault_tolerance(FaultToleranceManager* ftm);
void destroy_fault_tolerance_manager(FaultToleranceManager* ftm);

// Monitoreo y recuperación
void* heartbeat_monitor_thread(void* arg);
void handle_node_failure(FaultToleranceManager* ftm, Node* failed_node);
void recover_node(FaultToleranceManager* ftm, int node_id);

// Checkpoints
void create_checkpoint(FaultToleranceManager* ftm, const char* checkpoint_name);
int restore_checkpoint(FaultToleranceManager* ftm, const char* checkpoint_name);

// Simulación y pruebas
void simulate_node_failure(FaultToleranceManager* ftm, int node_id);

// Estadísticas
void print_fault_tolerance_stats(FaultToleranceManager* ftm);

#endif // FAULT_MANAGER_H