#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../common.h"

// ========================================
// ESTRUCTURAS DEL SCHEDULER
// ========================================

typedef struct {
    Task tasks[MAX_TASKS];
    int task_count;
    int next_task_id;
    pthread_mutex_t scheduler_lock;
} DistributedScheduler;

// ========================================
// FUNCIONES PÚBLICAS
// ========================================

// Inicialización
void init_scheduler();
void cleanup_scheduler();

// Gestión de tareas
int schedule_task(Task* task, Node nodes[], int node_count);
void update_task_status(int task_id, TaskStatus new_status);
int get_pending_tasks_count();
void reschedule_failed_tasks(Node nodes[], int node_count);

// Asignación inteligente
float calculate_node_score(Node* node);
int assign_task_to_node(Task* task, Node nodes[], int node_count);

// Estadísticas
void print_scheduler_stats();

// Variable global
extern DistributedScheduler* scheduler;

#endif // SCHEDULER_H