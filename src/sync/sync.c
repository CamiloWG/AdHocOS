#include "../common.h"
#include "sync.h"

// ========================================
// SINCRONIZACIÓN DISTRIBUIDA (Algoritmo de Lamport)
// ========================================

LamportMutex* create_lamport_mutex(int node_id) {
    LamportMutex* mutex = (LamportMutex*)malloc(sizeof(LamportMutex));
    mutex->timestamp = 0;
    mutex->node_id = node_id;
    mutex->requesting = 0;
    mutex->granted = 0;
    mutex->replies_received = 0;
    pthread_mutex_init(&mutex->lock, NULL);
    pthread_cond_init(&mutex->cond, NULL);
    
    log_info("Mutex de Lamport creado para nodo %d", node_id);
    return mutex;
}

void acquire_distributed_lock(LamportMutex* mutex, int total_nodes) {
    pthread_mutex_lock(&mutex->lock);
    
    mutex->requesting = 1;
    mutex->timestamp++;
    mutex->replies_received = 0;
    
    log_info("🔒 Nodo %d solicitando lock (timestamp: %d)", 
             mutex->node_id, mutex->timestamp);
    
    // En implementación real, enviaríamos REQUEST a todos los nodos
    // Aquí simulamos espera de respuestas
    
    // Esperar confirmación de todos los nodos
    while (mutex->replies_received < total_nodes - 1) {
        // Timeout simulado
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2; // 2 segundos timeout
        
        int result = pthread_cond_timedwait(&mutex->cond, &mutex->lock, &ts);
        if (result == ETIMEDOUT) {
            log_debug("Timeout esperando respuestas, asumiendo granted");
            mutex->replies_received = total_nodes - 1;
        }
    }
    
    mutex->granted = 1;
    log_info("✅ Nodo %d obtuvo el lock", mutex->node_id);
    
    pthread_mutex_unlock(&mutex->lock);
}

void release_distributed_lock(LamportMutex* mutex) {
    pthread_mutex_lock(&mutex->lock);
    
    mutex->requesting = 0;
    mutex->granted = 0;
    
    log_info("🔓 Nodo %d liberando lock", mutex->node_id);
    
    pthread_cond_broadcast(&mutex->cond);
    pthread_mutex_unlock(&mutex->lock);
}

void handle_lock_request(LamportMutex* mutex, int requesting_node, int request_timestamp) {
    pthread_mutex_lock(&mutex->lock);
    
    // Comparar timestamps (algoritmo de Lamport)
    int should_grant = 0;
    
    if (!mutex->requesting) {
        should_grant = 1;
    } else if (request_timestamp < mutex->timestamp) {
        should_grant = 1;
    } else if (request_timestamp == mutex->timestamp && requesting_node < mutex->node_id) {
        should_grant = 1;
    }
    
    if (should_grant) {
        log_debug("Nodo %d: Concediendo lock a nodo %d", 
                  mutex->node_id, requesting_node);
        // Enviar REPLY al nodo solicitante
    } else {
        log_debug("Nodo %d: Postergando respuesta a nodo %d", 
                  mutex->node_id, requesting_node);
    }
    
    pthread_mutex_unlock(&mutex->lock);
}

void handle_lock_reply(LamportMutex* mutex) {
    pthread_mutex_lock(&mutex->lock);
    
    mutex->replies_received++;
    
    if (mutex->replies_received >= mutex->replies_received) {
        pthread_cond_signal(&mutex->cond);
    }
    
    pthread_mutex_unlock(&mutex->lock);
}

void destroy_lamport_mutex(LamportMutex* mutex) {
    if (mutex) {
        pthread_mutex_destroy(&mutex->lock);
        pthread_cond_destroy(&mutex->cond);
        free(mutex);
    }
}

// ========================================
// BARRERAS DISTRIBUIDAS
// ========================================

DistributedBarrier* create_distributed_barrier(int node_id, int total_nodes) {
    DistributedBarrier* barrier = (DistributedBarrier*)malloc(sizeof(DistributedBarrier));
    barrier->node_id = node_id;
    barrier->total_nodes = total_nodes;
    barrier->arrived_count = 0;
    barrier->generation = 0;
    pthread_mutex_init(&barrier->lock, NULL);
    pthread_cond_init(&barrier->cond, NULL);
    
    memset(barrier->arrived_nodes, 0, sizeof(barrier->arrived_nodes));
    
    log_info("Barrera distribuida creada para nodo %d (%d nodos totales)", 
             node_id, total_nodes);
    return barrier;
}

void wait_at_barrier(DistributedBarrier* barrier) {
    pthread_mutex_lock(&barrier->lock);
    
    int my_generation = barrier->generation;
    barrier->arrived_nodes[barrier->node_id] = 1;
    barrier->arrived_count++;
    
    log_info("🚧 Nodo %d esperando en barrera (%d/%d)", 
             barrier->node_id, barrier->arrived_count, barrier->total_nodes);
    
    if (barrier->arrived_count == barrier->total_nodes) {
        // Todos llegaron, liberar barrera
        log_info("✅ Barrera liberada (generación %d)", barrier->generation);
        barrier->generation++;
        barrier->arrived_count = 0;
        memset(barrier->arrived_nodes, 0, sizeof(barrier->arrived_nodes));
        pthread_cond_broadcast(&barrier->cond);
    } else {
        // Esperar a que todos lleguen
        while (barrier->generation == my_generation) {
            pthread_cond_wait(&barrier->cond, &barrier->lock);
        }
    }
    
    pthread_mutex_unlock(&barrier->lock);
}

void destroy_distributed_barrier(DistributedBarrier* barrier) {
    if (barrier) {
        pthread_mutex_destroy(&barrier->lock);
        pthread_cond_destroy(&barrier->cond);
        free(barrier);
    }
}

// ========================================
// RELOJ LÓGICO DE LAMPORT
// ========================================

LogicalClock* create_logical_clock(int node_id) {
    LogicalClock* clock = (LogicalClock*)malloc(sizeof(LogicalClock));
    clock->timestamp = 0;
    clock->node_id = node_id;
    pthread_mutex_init(&clock->lock, NULL);
    
    log_debug("Reloj lógico creado para nodo %d", node_id);
    return clock;
}

int increment_clock(LogicalClock* clock) {
    pthread_mutex_lock(&clock->lock);
    clock->timestamp++;
    int ts = clock->timestamp;
    pthread_mutex_unlock(&clock->lock);
    return ts;
}

int update_clock(LogicalClock* clock, int received_timestamp) {
    pthread_mutex_lock(&clock->lock);
    
    if (received_timestamp > clock->timestamp) {
        clock->timestamp = received_timestamp;
    }
    clock->timestamp++;
    
    int ts = clock->timestamp;
    pthread_mutex_unlock(&clock->lock);
    return ts;
}

int get_clock_time(LogicalClock* clock) {
    pthread_mutex_lock(&clock->lock);
    int ts = clock->timestamp;
    pthread_mutex_unlock(&clock->lock);
    return ts;
}

void destroy_logical_clock(LogicalClock* clock) {
    if (clock) {
        pthread_mutex_destroy(&clock->lock);
        free(clock);
    }
}