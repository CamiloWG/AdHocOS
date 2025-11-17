// ========================================
// ESTRUCTURA BASE DEL SISTEMA OPERATIVO DESCENTRALIZADO
// Fase 2: Núcleo Funcional Distribuido
// ========================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// ========================================
// 1. ESTRUCTURAS DE DATOS PRINCIPALES
// ========================================

#define MAX_NODES 100
#define MAX_TASKS 1000
#define BUFFER_SIZE 1024
#define NODE_PORT 8080

// Estados de los nodos
typedef enum {
    NODE_IDLE,
    NODE_BUSY,
    NODE_OFFLINE,
    NODE_FAILED
} NodeStatus;

// Estructura para representar un nodo en la red
typedef struct {
    int node_id;
    char ip_address[16];
    int port;
    NodeStatus status;
    float cpu_load;
    float memory_usage;
    float reputation;
    time_t last_heartbeat;
} Node;

// Estructura para tareas distribuidas
typedef struct {
    int task_id;
    int priority;
    int assigned_node;
    void* (*task_function)(void*);
    void* task_data;
    int data_size;
    int status; // 0: pending, 1: running, 2: completed, 3: failed
    time_t creation_time;
    time_t completion_time;
} Task;

// Estructura para memoria compartida distribuida
typedef struct {
    int memory_id;
    void* data;
    size_t size;
    int owner_node;
    int reference_count;
    pthread_mutex_t lock;
} SharedMemory;

// ========================================
// 2. SCHEDULER DISTRIBUIDO
// ========================================

typedef struct {
    Task tasks[MAX_TASKS];
    int task_count;
    pthread_mutex_t scheduler_lock;
} DistributedScheduler;

DistributedScheduler* scheduler;

// Inicializar el scheduler
void init_scheduler() {
    scheduler = malloc(sizeof(DistributedScheduler));
    scheduler->task_count = 0;
    pthread_mutex_init(&scheduler->scheduler_lock, NULL);
}

// Algoritmo de asignación de tareas basado en múltiples criterios
int assign_task_to_node(Task* task, Node nodes[], int node_count) {
    int best_node = -1;
    float best_score = -1;
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].status == NODE_IDLE || nodes[i].status == NODE_BUSY) {
            // Calcular puntuación basada en:
            // - Reputación (40%)
            // - Carga de CPU (30%)
            // - Uso de memoria (30%)
            float score = (nodes[i].reputation * 0.4) + 
                         ((1.0 - nodes[i].cpu_load) * 0.3) + 
                         ((1.0 - nodes[i].memory_usage) * 0.3);
            
            if (score > best_score) {
                best_score = score;
                best_node = i;
            }
        }
    }
    
    return best_node;
}

// Añadir tarea al scheduler
int schedule_task(Task* task, Node nodes[], int node_count) {
    pthread_mutex_lock(&scheduler->scheduler_lock);
    
    if (scheduler->task_count >= MAX_TASKS) {
        pthread_mutex_unlock(&scheduler->scheduler_lock);
        return -1;
    }
    
    int node_id = assign_task_to_node(task, nodes, node_count);
    if (node_id == -1) {
        pthread_mutex_unlock(&scheduler->scheduler_lock);
        return -1;
    }
    
    task->assigned_node = node_id;
    task->status = 1; // Running
    scheduler->tasks[scheduler->task_count++] = *task;
    
    pthread_mutex_unlock(&scheduler->scheduler_lock);
    return node_id;
}

// ========================================
// 3. GESTIÓN DE MEMORIA DISTRIBUIDA
// ========================================

typedef struct {
    SharedMemory* memory_blocks[1000];
    int block_count;
    pthread_mutex_t memory_lock;
} DistributedMemoryManager;

DistributedMemoryManager* memory_manager;

// Inicializar gestor de memoria
void init_memory_manager() {
    memory_manager = malloc(sizeof(DistributedMemoryManager));
    memory_manager->block_count = 0;
    pthread_mutex_init(&memory_manager->memory_lock, NULL);
}

// Asignar memoria compartida
SharedMemory* allocate_shared_memory(size_t size, int owner_node) {
    pthread_mutex_lock(&memory_manager->memory_lock);
    
    SharedMemory* mem = malloc(sizeof(SharedMemory));
    mem->data = malloc(size);
    mem->size = size;
    mem->owner_node = owner_node;
    mem->reference_count = 1;
    mem->memory_id = memory_manager->block_count;
    pthread_mutex_init(&mem->lock, NULL);
    
    memory_manager->memory_blocks[memory_manager->block_count++] = mem;
    
    pthread_mutex_unlock(&memory_manager->memory_lock);
    return mem;
}

// Replicar datos entre nodos
int replicate_memory(SharedMemory* mem, int target_node) {
    // Simulación de replicación
    pthread_mutex_lock(&mem->lock);
    mem->reference_count++;
    pthread_mutex_unlock(&mem->lock);
    
    printf("Replicando memoria %d al nodo %d\n", mem->memory_id, target_node);
    return 0;
}

// ========================================
// 4. SINCRONIZACIÓN DISTRIBUIDA
// ========================================

typedef struct {
    int lock_id;
    int owner_node;
    int waiting_nodes[MAX_NODES];
    int waiting_count;
    pthread_mutex_t internal_lock;
} DistributedLock;

// Mutex distribuido usando algoritmo de Lamport
typedef struct {
    int timestamp;
    int node_id;
    int requesting;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} LamportMutex;

// Inicializar mutex de Lamport
LamportMutex* create_lamport_mutex(int node_id) {
    LamportMutex* mutex = malloc(sizeof(LamportMutex));
    mutex->timestamp = 0;
    mutex->node_id = node_id;
    mutex->requesting = 0;
    pthread_mutex_init(&mutex->lock, NULL);
    pthread_cond_init(&mutex->cond, NULL);
    return mutex;
}

// Adquirir mutex distribuido
void acquire_distributed_lock(LamportMutex* mutex) {
    pthread_mutex_lock(&mutex->lock);
    mutex->requesting = 1;
    mutex->timestamp++;
    
    // Enviar solicitud a todos los nodos
    // (Implementación simplificada)
    printf("Nodo %d solicitando lock con timestamp %d\n", 
           mutex->node_id, mutex->timestamp);
    
    // Esperar confirmación
    pthread_cond_wait(&mutex->cond, &mutex->lock);
    pthread_mutex_unlock(&mutex->lock);
}

// Liberar mutex distribuido
void release_distributed_lock(LamportMutex* mutex) {
    pthread_mutex_lock(&mutex->lock);
    mutex->requesting = 0;
    pthread_cond_broadcast(&mutex->cond);
    pthread_mutex_unlock(&mutex->lock);
    
    printf("Nodo %d liberando lock\n", mutex->node_id);
}

// ========================================
// 5. TOLERANCIA A FALLOS
// ========================================

typedef struct {
    Node* nodes;
    int node_count;
    pthread_t heartbeat_thread;
    int running;
} FaultToleranceManager;

// Verificar heartbeat de nodos
void* heartbeat_monitor(void* arg) {
    FaultToleranceManager* ftm = (FaultToleranceManager*)arg;
    
    while (ftm->running) {
        time_t current_time = time(NULL);
        
        for (int i = 0; i < ftm->node_count; i++) {
            if (current_time - ftm->nodes[i].last_heartbeat > 10) {
                // Nodo no responde
                if (ftm->nodes[i].status != NODE_FAILED) {
                    printf("Nodo %d detectado como fallido\n", 
                           ftm->nodes[i].node_id);
                    ftm->nodes[i].status = NODE_FAILED;
                    
                    // Iniciar proceso de recuperación
                    handle_node_failure(&ftm->nodes[i]);
                }
            }
        }
        
        sleep(5); // Verificar cada 5 segundos
    }
    
    return NULL;
}

// Manejar fallo de nodo
void handle_node_failure(Node* failed_node) {
    printf("Iniciando recuperación para nodo %d\n", failed_node->node_id);
    
    // 1. Reasignar tareas del nodo fallido
    for (int i = 0; i < scheduler->task_count; i++) {
        if (scheduler->tasks[i].assigned_node == failed_node->node_id &&
            scheduler->tasks[i].status == 1) {
            // Marcar tarea para reasignación
            scheduler->tasks[i].status = 0;
            printf("Tarea %d marcada para reasignación\n", 
                   scheduler->tasks[i].task_id);
        }
    }
    
    // 2. Replicar datos críticos
    // 3. Actualizar tabla de enrutamiento
    // 4. Notificar a otros nodos
}

// ========================================
// 6. COMUNICACIÓN ENTRE NODOS
// ========================================

typedef struct {
    int type; // 0: heartbeat, 1: task, 2: data, 3: sync
    int source_node;
    int dest_node;
    char data[BUFFER_SIZE];
    int data_size;
} Message;

// Enviar mensaje a otro nodo
int send_message(Node* dest_node, Message* msg) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dest_node->port);
    server_addr.sin_addr.s_addr = inet_addr(dest_node->ip_address);
    
    if (connect(sockfd, (struct sockaddr*)&server_addr, 
                sizeof(server_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    send(sockfd, msg, sizeof(Message), 0);
    close(sockfd);
    
    return 0;
}

// ========================================
// 7. FUNCIONES PRINCIPALES DEL KERNEL
// ========================================

typedef struct {
    int node_id;
    Node nodes[MAX_NODES];
    int node_count;
    DistributedScheduler* scheduler;
    DistributedMemoryManager* memory_manager;
    FaultToleranceManager* ft_manager;
    int running;
} DecentralizedKernel;

DecentralizedKernel* kernel;

// Inicializar kernel descentralizado
void init_kernel(int node_id) {
    kernel = malloc(sizeof(DecentralizedKernel));
    kernel->node_id = node_id;
    kernel->node_count = 0;
    kernel->running = 1;
    
    // Inicializar subsistemas
    init_scheduler();
    init_memory_manager();
    
    // Inicializar gestor de tolerancia a fallos
    kernel->ft_manager = malloc(sizeof(FaultToleranceManager));
    kernel->ft_manager->nodes = kernel->nodes;
    kernel->ft_manager->node_count = kernel->node_count;
    kernel->ft_manager->running = 1;
    
    printf("Kernel descentralizado inicializado - Nodo ID: %d\n", node_id);
}

// Descubrir nodos en la red
void discover_nodes() {
    printf("Descubriendo nodos en la red Ad hoc...\n");
    
    // Implementación simplificada
    // En una implementación real, usarías broadcast/multicast
    // para descubrir nodos activos
    
    // Simulación: agregar algunos nodos de prueba
    for (int i = 0; i < 3; i++) {
        if (i != kernel->node_id) {
            Node new_node;
            new_node.node_id = i;
            sprintf(new_node.ip_address, "192.168.1.%d", 100 + i);
            new_node.port = NODE_PORT + i;
            new_node.status = NODE_IDLE;
            new_node.cpu_load = 0.2 + (i * 0.1);
            new_node.memory_usage = 0.3 + (i * 0.1);
            new_node.reputation = 0.8 + (i * 0.05);
            new_node.last_heartbeat = time(NULL);
            
            kernel->nodes[kernel->node_count++] = new_node;
            printf("Nodo descubierto: ID=%d, IP=%s\n", 
                   new_node.node_id, new_node.ip_address);
        }
    }
}

// ========================================
// 8. FUNCIÓN PRINCIPAL
// ========================================

int main(int argc, char* argv[]) {
    int node_id = 0;
    
    if (argc > 1) {
        node_id = atoi(argv[1]);
    }
    
    printf("========================================\n");
    printf("Sistema Operativo Descentralizado v0.1\n");
    printf("========================================\n\n");
    
    // Inicializar kernel
    init_kernel(node_id);
    
    // Descubrir nodos en la red
    discover_nodes();
    
    // Iniciar monitor de heartbeat
    pthread_create(&kernel->ft_manager->heartbeat_thread, NULL, 
                   heartbeat_monitor, kernel->ft_manager);
    
    // Ejemplo: Crear y programar una tarea
    Task example_task;
    example_task.task_id = 1;
    example_task.priority = 5;
    example_task.status = 0;
    example_task.creation_time = time(NULL);
    
    int assigned = schedule_task(&example_task, kernel->nodes, kernel->node_count);
    if (assigned >= 0) {
        printf("\nTarea %d asignada al nodo %d\n", 
               example_task.task_id, assigned);
    }
    
    // Ejemplo: Asignar memoria compartida
    SharedMemory* shared_mem = allocate_shared_memory(1024, node_id);
    printf("Memoria compartida asignada: ID=%d, Tamaño=%zu bytes\n", 
           shared_mem->memory_id, shared_mem->size);
    
    // Simular ejecución del kernel
    printf("\nKernel en ejecución... (Presiona Ctrl+C para salir)\n");
    
    while (kernel->running) {
        sleep(1);
        // Aquí irían las operaciones principales del kernel
    }
    
    // Limpieza
    kernel->ft_manager->running = 0;
    pthread_join(kernel->ft_manager->heartbeat_thread, NULL);
    
    free(scheduler);
    free(memory_manager);
    free(kernel->ft_manager);
    free(kernel);
    
    return 0;
}