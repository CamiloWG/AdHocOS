#include "../common.h"
#include "scheduler.h"
#include <stdarg.h>

// ========================================
// IMPLEMENTACIÓN DEL SCHEDULER DISTRIBUIDO
// ========================================

DistributedScheduler* scheduler = NULL;

void init_scheduler() {
    scheduler = (DistributedScheduler*)malloc(sizeof(DistributedScheduler));
    scheduler->task_count = 0;
    scheduler->next_task_id = 1;
    pthread_mutex_init(&scheduler->scheduler_lock, NULL);
    
    printf("[INFO] Scheduler distribuido inicializado\n");
}

// Calcular score de un nodo para asignar tarea
float calculate_node_score(Node* node) {
    if (node->status == NODE_FAILED || node->status == NODE_OFFLINE) {
        return -1.0;
    }
    
    // Score basado en múltiples factores:
    // - Reputación (40%): Historial de tareas completadas exitosamente
    // - Carga CPU (30%): Menor carga = mejor
    // - Memoria disponible (30%): Más memoria = mejor
    float score = (node->reputation * 0.4) + 
                  ((1.0 - node->cpu_load) * 0.3) + 
                  ((1.0 - node->memory_usage) * 0.3);
    
    return score;
}

int assign_task_to_node(Task* task, Node nodes[], int node_count) {
    int best_node = -1;
    float best_score = -1.0;
    
    for (int i = 0; i < node_count; i++) {
        float score = calculate_node_score(&nodes[i]);
        
        if (score > best_score) {
            best_score = score;
            best_node = i;
        }
    }
    
    if (best_node >= 0) {
        printf("[INFO] Tarea %d asignada al nodo %d (score: %.2f)\n", 
               task->task_id, nodes[best_node].node_id, best_score);
    }
    
    return best_node;
}

int schedule_task(Task* task, Node nodes[], int node_count) {
    pthread_mutex_lock(&scheduler->scheduler_lock);
    
    if (scheduler->task_count >= MAX_TASKS) {
        printf("[ERROR] Cola de tareas llena\n");
        pthread_mutex_unlock(&scheduler->scheduler_lock);
        return -1;
    }
    
    // Asignar ID si no tiene
    if (task->task_id == 0) {
        task->task_id = scheduler->next_task_id++;
    }
    
    // Encontrar mejor nodo
    int node_idx = assign_task_to_node(task, nodes, node_count);
    if (node_idx == -1) {
        printf("[ERROR] No hay nodos disponibles para tarea %d\n", task->task_id);
        pthread_mutex_unlock(&scheduler->scheduler_lock);
        return -1;
    }
    
    // Asignar tarea
    task->assigned_node = nodes[node_idx].node_id;
    task->status = TASK_RUNNING;
    task->creation_time = time(NULL);
    
    // Guardar en cola
    scheduler->tasks[scheduler->task_count++] = *task;
    
    pthread_mutex_unlock(&scheduler->scheduler_lock);
    return nodes[node_idx].node_id;
}

void update_task_status(int task_id, TaskStatus new_status) {
    pthread_mutex_lock(&scheduler->scheduler_lock);
    
    for (int i = 0; i < scheduler->task_count; i++) {
        if (scheduler->tasks[i].task_id == task_id) {
            scheduler->tasks[i].status = new_status;
            if (new_status == TASK_COMPLETED || new_status == TASK_FAILED) {
                scheduler->tasks[i].completion_time = time(NULL);
            }
            printf("[INFO] Tarea %d actualizada a estado %d\n", task_id, new_status);
            break;
        }
    }
    
    pthread_mutex_unlock(&scheduler->scheduler_lock);
}

int get_pending_tasks_count() {
    int count = 0;
    pthread_mutex_lock(&scheduler->scheduler_lock);
    
    for (int i = 0; i < scheduler->task_count; i++) {
        if (scheduler->tasks[i].status == TASK_PENDING) {
            count++;
        }
    }
    
    pthread_mutex_unlock(&scheduler->scheduler_lock);
    return count;
}

void reschedule_failed_tasks(Node nodes[], int node_count) {
    pthread_mutex_lock(&scheduler->scheduler_lock);
    
    for (int i = 0; i < scheduler->task_count; i++) {
        if (scheduler->tasks[i].status == TASK_FAILED || 
            scheduler->tasks[i].status == TASK_PENDING) {
            
            int node_idx = assign_task_to_node(&scheduler->tasks[i], nodes, node_count);
            if (node_idx >= 0) {
                scheduler->tasks[i].assigned_node = nodes[node_idx].node_id;
                scheduler->tasks[i].status = TASK_RUNNING;
                printf("[INFO] Tarea %d reasignada al nodo %d\n", 
                       scheduler->tasks[i].task_id, nodes[node_idx].node_id);
            }
        }
    }
    
    pthread_mutex_unlock(&scheduler->scheduler_lock);
}

void print_scheduler_stats() {
    pthread_mutex_lock(&scheduler->scheduler_lock);
    
    int pending = 0, running = 0, completed = 0, failed = 0;
    
    for (int i = 0; i < scheduler->task_count; i++) {
        switch(scheduler->tasks[i].status) {
            case TASK_PENDING: pending++; break;
            case TASK_RUNNING: running++; break;
            case TASK_COMPLETED: completed++; break;
            case TASK_FAILED: failed++; break;
        }
    }
    
    printf("[INFO] 📊 Estadísticas Scheduler:\n");
    printf("[INFO]    Total: %d | Pendientes: %d | Ejecutando: %d | Completadas: %d | Fallidas: %d\n",
           scheduler->task_count, pending, running, completed, failed);
    
    pthread_mutex_unlock(&scheduler->scheduler_lock);
}

void cleanup_scheduler() {
    if (scheduler) {
        pthread_mutex_destroy(&scheduler->scheduler_lock);
        free(scheduler);
        scheduler = NULL;
    }
}