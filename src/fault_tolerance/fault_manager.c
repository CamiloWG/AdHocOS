#include "../common.h"
#include "../scheduler/scheduler.h"
#include "../memory/memory_manager.h"
#include "fault_manager.h"

// ========================================
// TOLERANCIA A FALLOS Y RECUPERACIÓN
// ========================================

void* heartbeat_monitor_thread(void* arg) {
    FaultToleranceManager* ftm = (FaultToleranceManager*)arg;
    
    log_info("💓 Monitor de heartbeat iniciado");
    
    while (ftm->running) {
        time_t current_time = time(NULL);
        
        pthread_mutex_lock(&ftm->lock);
        
        for (int i = 0; i < ftm->node_count; i++) {
            Node* node = &ftm->nodes[i];
            
            // Verificar timeout
            if (current_time - node->last_heartbeat > NODE_TIMEOUT) {
                if (node->status != NODE_FAILED && node->status != NODE_OFFLINE) {
                    log_error("⚠️  Nodo %d no responde (timeout)", node->node_id);
                    node->status = NODE_FAILED;
                    ftm->failed_nodes++;
                    
                    // Iniciar recuperación
                    handle_node_failure(ftm, node);
                }
            }
        }
        
        pthread_mutex_unlock(&ftm->lock);
        
        sleep(HEARTBEAT_INTERVAL);
    }
    
    return NULL;
}

void handle_node_failure(FaultToleranceManager* ftm, Node* failed_node) {
    log_info("🔧 Iniciando recuperación para nodo %d", failed_node->node_id);
    
    // 1. Reasignar tareas del nodo fallido
    if (scheduler) {
        pthread_mutex_lock(&scheduler->scheduler_lock);
        
        for (int i = 0; i < scheduler->task_count; i++) {
            Task* task = &scheduler->tasks[i];
            if (task->assigned_node == failed_node->node_id && 
                task->status == TASK_RUNNING) {
                
                log_info("   → Tarea %d marcada para reasignación", task->task_id);
                task->status = TASK_PENDING;
                ftm->tasks_recovered++;
            }
        }
        
        pthread_mutex_unlock(&scheduler->scheduler_lock);
        
        // Reasignar tareas pendientes
        reschedule_failed_tasks(ftm->nodes, ftm->node_count);
    }
    
    // 2. Verificar y replicar datos críticos
    if (memory_manager) {
        pthread_mutex_lock(&memory_manager->memory_lock);
        
        for (int i = 0; i < memory_manager->block_count; i++) {
            SharedMemory* mem = memory_manager->memory_blocks[i];
            
            if (mem->owner_node == failed_node->node_id) {
                log_info("   → Memoria %d necesita nueva réplica", mem->memory_id);
                
                // Encontrar nodo disponible para réplica
                for (int j = 0; j < ftm->node_count; j++) {
                    if (ftm->nodes[j].status == NODE_IDLE && 
                        ftm->nodes[j].node_id != failed_node->node_id) {
                        replicate_memory(mem, ftm->nodes[j].node_id);
                        break;
                    }
                }
            }
        }
        
        pthread_mutex_unlock(&memory_manager->memory_lock);
    }
    
    // 3. Actualizar reputación del nodo
    failed_node->reputation *= 0.5; // Penalizar
    
    // 4. Notificar a otros nodos (simulado)
    log_info("   → Notificando fallo a otros nodos");
    
    log_info("✅ Recuperación completada para nodo %d", failed_node->node_id);
}

void recover_node(FaultToleranceManager* ftm, int node_id) {
    pthread_mutex_lock(&ftm->lock);
    
    for (int i = 0; i < ftm->node_count; i++) {
        if (ftm->nodes[i].node_id == node_id) {
            if (ftm->nodes[i].status == NODE_FAILED) {
                log_info("♻️  Recuperando nodo %d", node_id);
                ftm->nodes[i].status = NODE_IDLE;
                ftm->nodes[i].last_heartbeat = time(NULL);
                ftm->nodes[i].reputation = 0.7; // Reputación reducida
                ftm->failed_nodes--;
                ftm->recoveries++;
                log_info("✅ Nodo %d recuperado exitosamente", node_id);
            }
            break;
        }
    }
    
    pthread_mutex_unlock(&ftm->lock);
}

void simulate_node_failure(FaultToleranceManager* ftm, int node_id) {
    log_info("🧪 Simulando fallo del nodo %d", node_id);
    
    pthread_mutex_lock(&ftm->lock);
    
    for (int i = 0; i < ftm->node_count; i++) {
        if (ftm->nodes[i].node_id == node_id) {
            ftm->nodes[i].status = NODE_FAILED;
            ftm->nodes[i].last_heartbeat = time(NULL) - NODE_TIMEOUT - 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&ftm->lock);
}

void create_checkpoint(FaultToleranceManager* ftm, const char* checkpoint_name) {
    pthread_mutex_lock(&ftm->lock);
    
    if (ftm->checkpoint_count >= 10) {
        log_error("Límite de checkpoints alcanzado");
        pthread_mutex_unlock(&ftm->lock);
        return;
    }
    
    Checkpoint* cp = &ftm->checkpoints[ftm->checkpoint_count++];
    strncpy(cp->name, checkpoint_name, 63);
    cp->timestamp = time(NULL);
    cp->node_count = ftm->node_count;
    
    // Copiar estado de nodos
    for (int i = 0; i < ftm->node_count; i++) {
        cp->node_states[i] = ftm->nodes[i];
    }
    
    log_info("💾 Checkpoint '%s' creado (%d nodos)", checkpoint_name, ftm->node_count);
    
    pthread_mutex_unlock(&ftm->lock);
}

int restore_checkpoint(FaultToleranceManager* ftm, const char* checkpoint_name) {
    pthread_mutex_lock(&ftm->lock);
    
    for (int i = 0; i < ftm->checkpoint_count; i++) {
        if (strcmp(ftm->checkpoints[i].name, checkpoint_name) == 0) {
            Checkpoint* cp = &ftm->checkpoints[i];
            
            log_info("♻️  Restaurando checkpoint '%s'", checkpoint_name);
            
            // Restaurar estado
            ftm->node_count = cp->node_count;
            for (int j = 0; j < cp->node_count; j++) {
                ftm->nodes[j] = cp->node_states[j];
            }
            
            pthread_mutex_unlock(&ftm->lock);
            return 0;
        }
    }
    
    log_error("Checkpoint '%s' no encontrado", checkpoint_name);
    pthread_mutex_unlock(&ftm->lock);
    return -1;
}

void print_fault_tolerance_stats(FaultToleranceManager* ftm) {
    pthread_mutex_lock(&ftm->lock);
    
    int active = 0, failed = 0, offline = 0;
    
    for (int i = 0; i < ftm->node_count; i++) {
        switch(ftm->nodes[i].status) {
            case NODE_IDLE:
            case NODE_BUSY:
                active++;
                break;
            case NODE_FAILED:
                failed++;
                break;
            case NODE_OFFLINE:
                offline++;
                break;
        }
    }
    
    log_info("📊 Estadísticas de Tolerancia a Fallos:");
    log_info("   Nodos: %d activos, %d fallidos, %d offline", active, failed, offline);
    log_info("   Recuperaciones: %d | Tareas recuperadas: %d", 
             ftm->recoveries, ftm->tasks_recovered);
    log_info("   Checkpoints: %d", ftm->checkpoint_count);
    
    pthread_mutex_unlock(&ftm->lock);
}

FaultToleranceManager* create_fault_tolerance_manager() {
    FaultToleranceManager* ftm = (FaultToleranceManager*)malloc(sizeof(FaultToleranceManager));
    ftm->node_count = 0;
    ftm->running = 0;
    ftm->failed_nodes = 0;
    ftm->recoveries = 0;
    ftm->tasks_recovered = 0;
    ftm->checkpoint_count = 0;
    pthread_mutex_init(&ftm->lock, NULL);
    
    log_info("Gestor de tolerancia a fallos creado");
    return ftm;
}

void start_fault_tolerance(FaultToleranceManager* ftm) {
    ftm->running = 1;
    pthread_create(&ftm->monitor_thread, NULL, heartbeat_monitor_thread, ftm);
    log_info("Tolerancia a fallos iniciada");
}

void stop_fault_tolerance(FaultToleranceManager* ftm) {
    if (ftm->running) {
        ftm->running = 0;
        pthread_join(ftm->monitor_thread, NULL);
        log_info("Tolerancia a fallos detenida");
    }
}

void destroy_fault_tolerance_manager(FaultToleranceManager* ftm) {
    if (ftm) {
        stop_fault_tolerance(ftm);
        pthread_mutex_destroy(&ftm->lock);
        free(ftm);
    }
}