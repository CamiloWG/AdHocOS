// main.c - Sistema Operativo Descentralizado
// Version corregida sin errores de compilación

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>  // IMPORTANTE: Para bool, true, false
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdatomic.h>

// ========================================
// DEFINICIONES Y TIPOS BASE
// ========================================

#define MAX_NODES 100
#define MAX_TASKS 1000
#define BUFFER_SIZE 4096
#define NODE_PORT 8080
#define KERNEL_VERSION 0x0001

typedef uint64_t node_id_t;
typedef uint64_t task_id_t;

// Estados de los nodos
typedef enum {
    NODE_IDLE,
    NODE_BUSY,
    NODE_OFFLINE,
    NODE_FAILED
} NodeStatus;

// ========================================
// ESTRUCTURAS PRINCIPALES
// ========================================

// Estructura para representar un nodo
typedef struct {
    node_id_t node_id;
    char ip_address[16];
    int port;
    NodeStatus status;
    float cpu_load;
    float memory_usage;
    float reputation;
    time_t last_heartbeat;
    pthread_mutex_t lock;
} Node;

// Estructura para tareas
typedef struct {
    task_id_t task_id;
    int priority;
    node_id_t assigned_node;
    void* (*task_function)(void*);
    void* task_data;
    size_t data_size;
    int status; // 0: pending, 1: running, 2: completed, 3: failed
    time_t creation_time;
    time_t completion_time;
} Task;

// Memoria compartida
typedef struct {
    uint64_t memory_id;
    void* data;
    size_t size;
    node_id_t owner_node;
    _Atomic int reference_count;
    pthread_rwlock_t rwlock;
    bool is_mmapped;
    char shm_name[256];
} SharedMemory64;

// ========================================
// SCHEDULER DISTRIBUIDO
// ========================================

typedef struct {
    Task* tasks;
    size_t task_capacity;
    _Atomic size_t task_count;
    pthread_mutex_t scheduler_lock;
    pthread_cond_t task_available;
    bool running;
} DistributedScheduler;

// ========================================
// GESTOR DE MEMORIA
// ========================================

typedef struct {
    SharedMemory64** memory_blocks;
    size_t block_capacity;
    _Atomic size_t block_count;
    pthread_rwlock_t memory_lock;
    uint64_t total_allocated;
    uint64_t total_freed;
} MemoryManager64;

// ========================================
// SISTEMA DE ARCHIVOS DISTRIBUIDO (SIMPLIFICADO)
// ========================================

typedef enum {
    DFS_FILE,
    DFS_DIRECTORY,
    DFS_SYMLINK
} dfs_file_type_t;

typedef struct dfs_inode {
    uint64_t inode_number;
    dfs_file_type_t type;
    uint64_t size;
    uint32_t mode;
    time_t created;
    time_t modified;
    node_id_t owner_node;
    uint8_t* data;
    struct dfs_inode* parent;
    struct dfs_inode** children;
    size_t child_count;
    char name[256];
} dfs_inode_t;

typedef struct {
    dfs_inode_t* root;
    pthread_rwlock_t fs_lock;
    uint64_t next_inode;
    struct {
        bool enable_compression;
        bool enable_encryption;
    } config;
} DistributedFileSystem;

// ========================================
// KERNEL DISTRIBUIDO
// ========================================

typedef struct {
    node_id_t node_id;
    uint64_t kernel_version;
    Node* nodes;
    size_t node_capacity;
    _Atomic size_t node_count;
    DistributedScheduler* scheduler;
    MemoryManager64* memory_manager;
    DistributedFileSystem* filesystem;
    bool running;
    pthread_t monitor_thread;
    pthread_t network_thread;
    struct {
        _Atomic uint64_t tasks_completed;
        _Atomic uint64_t tasks_failed;
        _Atomic uint64_t messages_sent;
        _Atomic uint64_t messages_received;
    } stats;
} DistributedKernel64;

// ========================================
// VARIABLES GLOBALES
// ========================================

static DistributedKernel64* g_kernel = NULL;
static DistributedFileSystem* g_filesystem = NULL;

// ========================================
// FUNCIONES DE UTILIDAD
// ========================================

static uint64_t get_timestamp_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// ========================================
// IMPLEMENTACIÓN DEL SCHEDULER
// ========================================

DistributedScheduler* create_scheduler(size_t capacity) {
    DistributedScheduler* sched = malloc(sizeof(DistributedScheduler));
    if (!sched) return NULL;
    
    sched->tasks = calloc(capacity, sizeof(Task));
    sched->task_capacity = capacity;
    atomic_store(&sched->task_count, 0);
    pthread_mutex_init(&sched->scheduler_lock, NULL);
    pthread_cond_init(&sched->task_available, NULL);
    sched->running = true;
    
    return sched;
}

int schedule_task(DistributedScheduler* sched, Task* task) {
    if (!sched || !task) return -1;
    
    pthread_mutex_lock(&sched->scheduler_lock);
    
    size_t count = atomic_load(&sched->task_count);
    if (count >= sched->task_capacity) {
        pthread_mutex_unlock(&sched->scheduler_lock);
        return -1;
    }
    
    // Encontrar mejor nodo
    node_id_t best_node = 0;
    float best_score = -1;
    
    for (size_t i = 0; i < g_kernel->node_count; i++) {
        if (g_kernel->nodes[i].status == NODE_IDLE || 
            g_kernel->nodes[i].status == NODE_BUSY) {
            
            float score = (g_kernel->nodes[i].reputation * 0.4) + 
                         ((1.0 - g_kernel->nodes[i].cpu_load) * 0.3) + 
                         ((1.0 - g_kernel->nodes[i].memory_usage) * 0.3);
            
            if (score > best_score) {
                best_score = score;
                best_node = g_kernel->nodes[i].node_id;
            }
        }
    }
    
    task->assigned_node = best_node;
    task->status = 1; // Running
    task->creation_time = time(NULL);
    
    sched->tasks[count] = *task;
    atomic_fetch_add(&sched->task_count, 1);
    
    pthread_cond_signal(&sched->task_available);
    pthread_mutex_unlock(&sched->scheduler_lock);
    
    printf("[SCHEDULER] Tarea %lu asignada al nodo %lu\n", 
           task->task_id, best_node);
    
    return 0;
}

// ========================================
// GESTIÓN DE MEMORIA DISTRIBUIDA
// ========================================

MemoryManager64* create_memory_manager(size_t capacity) {
    MemoryManager64* mm = malloc(sizeof(MemoryManager64));
    if (!mm) return NULL;
    
    mm->memory_blocks = calloc(capacity, sizeof(SharedMemory64*));
    mm->block_capacity = capacity;
    atomic_store(&mm->block_count, 0);
    pthread_rwlock_init(&mm->memory_lock, NULL);
    mm->total_allocated = 0;
    mm->total_freed = 0;
    
    return mm;
}

SharedMemory64* allocate_shared_memory(size_t size, node_id_t owner) {
    if (!g_kernel || !g_kernel->memory_manager) return NULL;
    
    SharedMemory64* mem = malloc(sizeof(SharedMemory64));
    if (!mem) return NULL;
    
    mem->data = calloc(1, size);
    if (!mem->data) {
        free(mem);
        return NULL;
    }
    
    mem->size = size;
    mem->owner_node = owner;
    atomic_store(&mem->reference_count, 1);
    pthread_rwlock_init(&mem->rwlock, NULL);
    mem->is_mmapped = false;
    
    pthread_rwlock_wrlock(&g_kernel->memory_manager->memory_lock);
    
    size_t count = atomic_load(&g_kernel->memory_manager->block_count);
    if (count < g_kernel->memory_manager->block_capacity) {
        mem->memory_id = count;
        g_kernel->memory_manager->memory_blocks[count] = mem;
        atomic_fetch_add(&g_kernel->memory_manager->block_count, 1);
        g_kernel->memory_manager->total_allocated += size;
    }
    
    pthread_rwlock_unlock(&g_kernel->memory_manager->memory_lock);
    
    printf("[MEMORY] Bloque %lu asignado: %zu bytes\n", mem->memory_id, size);
    
    return mem;
}

// ========================================
// SISTEMA DE ARCHIVOS DISTRIBUIDO
// ========================================

DistributedFileSystem* dfs_init() {
    DistributedFileSystem* dfs = malloc(sizeof(DistributedFileSystem));
    if (!dfs) return NULL;
    
    // Crear nodo raíz
    dfs->root = calloc(1, sizeof(dfs_inode_t));
    dfs->root->inode_number = 0;
    dfs->root->type = DFS_DIRECTORY;
    strcpy(dfs->root->name, "/");
    dfs->root->mode = 0755;
    dfs->root->created = time(NULL);
    dfs->root->modified = time(NULL);
    
    pthread_rwlock_init(&dfs->fs_lock, NULL);
    dfs->next_inode = 1;
    dfs->config.enable_compression = false;
    dfs->config.enable_encryption = false;
    
    return dfs;
}

dfs_inode_t* dfs_create_file(const char* path, uint32_t mode) {
    if (!g_filesystem) return NULL;
    
    pthread_rwlock_wrlock(&g_filesystem->fs_lock);
    
    dfs_inode_t* file = calloc(1, sizeof(dfs_inode_t));
    file->inode_number = g_filesystem->next_inode++;
    file->type = DFS_FILE;
    strncpy(file->name, path, 255);
    file->mode = mode;
    file->created = time(NULL);
    file->modified = time(NULL);
    file->size = 0;
    file->owner_node = g_kernel->node_id;
    
    pthread_rwlock_unlock(&g_filesystem->fs_lock);
    
    printf("[DFS] Archivo creado: %s (inode %lu)\n", path, file->inode_number);
    
    return file;
}

ssize_t dfs_write(dfs_inode_t* file, const void* data, size_t size, off_t offset) {
    if (!file || file->type != DFS_FILE) return -1;
    
    pthread_rwlock_wrlock(&g_filesystem->fs_lock);
    
    // Expandir si es necesario
    size_t new_size = offset + size;
    if (new_size > file->size) {
        file->data = realloc(file->data, new_size);
        file->size = new_size;
    }
    
    memcpy(file->data + offset, data, size);
    file->modified = time(NULL);
    
    pthread_rwlock_unlock(&g_filesystem->fs_lock);
    
    return size;
}

// ========================================
// TOLERANCIA A FALLOS
// ========================================

void* heartbeat_monitor(void* arg) {
    DistributedKernel64* kernel = (DistributedKernel64*)arg;
    
    while (kernel->running) {
        time_t current_time = time(NULL);
        
        for (size_t i = 0; i < kernel->node_count; i++) {
            pthread_mutex_lock(&kernel->nodes[i].lock);
            
            if (current_time - kernel->nodes[i].last_heartbeat > 10) {
                if (kernel->nodes[i].status != NODE_FAILED) {
                    printf("[MONITOR] Nodo %lu detectado como fallido\n", 
                           kernel->nodes[i].node_id);
                    kernel->nodes[i].status = NODE_FAILED;
                    
                    // TODO: Implementar recuperación
                }
            }
            
            pthread_mutex_unlock(&kernel->nodes[i].lock);
        }
        
        sleep(5);
    }
    
    return NULL;
}

// ========================================
// INICIALIZACIÓN DEL KERNEL
// ========================================

DistributedKernel64* init_kernel(node_id_t node_id) {
    DistributedKernel64* kernel = calloc(1, sizeof(DistributedKernel64));
    if (!kernel) return NULL;
    
    kernel->node_id = node_id;
    kernel->kernel_version = KERNEL_VERSION;
    kernel->running = true;
    
    // Inicializar arrays
    kernel->node_capacity = MAX_NODES;
    kernel->nodes = calloc(kernel->node_capacity, sizeof(Node));
    atomic_store(&kernel->node_count, 0);
    
    // Crear subsistemas
    kernel->scheduler = create_scheduler(MAX_TASKS);
    kernel->memory_manager = create_memory_manager(1000);
    kernel->filesystem = dfs_init();
    
    // Asignar globales
    g_kernel = kernel;
    g_filesystem = kernel->filesystem;
    
    // Inicializar estadísticas
    atomic_store(&kernel->stats.tasks_completed, 0);
    atomic_store(&kernel->stats.tasks_failed, 0);
    atomic_store(&kernel->stats.messages_sent, 0);
    atomic_store(&kernel->stats.messages_received, 0);
    
    printf("[KERNEL] Sistema inicializado - Nodo ID: %lu\n", node_id);
    printf("[KERNEL] Versión: 0x%04lX\n", kernel->kernel_version);
    
    return kernel;
}

// ========================================
// DESCUBRIMIENTO DE NODOS
// ========================================

void discover_nodes() {
    printf("[DISCOVERY] Buscando nodos en la red...\n");
    
    // Simulación: agregar nodos de prueba
    for (int i = 0; i < 3; i++) {
        if ((node_id_t)i != g_kernel->node_id) {
            Node new_node = {0};
            new_node.node_id = i;
            sprintf(new_node.ip_address, "192.168.1.%d", 100 + i);
            new_node.port = NODE_PORT + i;
            new_node.status = NODE_IDLE;
            new_node.cpu_load = 0.2 + (i * 0.1);
            new_node.memory_usage = 0.3 + (i * 0.1);
            new_node.reputation = 0.8 + (i * 0.05);
            new_node.last_heartbeat = time(NULL);
            pthread_mutex_init(&new_node.lock, NULL);
            
            size_t idx = atomic_fetch_add(&g_kernel->node_count, 1);
            g_kernel->nodes[idx] = new_node;
            
            printf("[DISCOVERY] Nodo encontrado: ID=%d, IP=%s\n", 
                   i, new_node.ip_address);
        }
    }
}

// ========================================
// DEMOS Y PRUEBAS
// ========================================

void* example_task_function(void* data) {
    int* value = (int*)data;
    printf("[TASK] Ejecutando tarea con valor: %d\n", *value);
    sleep(2);
    return NULL;
}

void demo_scheduler() {
    printf("\n=== Demo: Scheduler Distribuido ===\n");
    
    for (int i = 0; i < 5; i++) {
        Task task = {0};
        task.task_id = i;
        task.priority = rand() % 10;
        task.task_function = example_task_function;
        
        int* data = malloc(sizeof(int));
        *data = i * 10;
        task.task_data = data;
        task.data_size = sizeof(int);
        
        schedule_task(g_kernel->scheduler, &task);
    }
}

void demo_memory() {
    printf("\n=== Demo: Memoria Distribuida ===\n");
    
    SharedMemory64* mem1 = allocate_shared_memory(1024, g_kernel->node_id);
    SharedMemory64* mem2 = allocate_shared_memory(4096, g_kernel->node_id);
    
    if (mem1) {
        strcpy((char*)mem1->data, "Datos compartidos de prueba");
        printf("[MEMORY] Datos escritos en bloque %lu\n", mem1->memory_id);
    }
    
    if (mem2) {
        memset(mem2->data, 0xAB, 100);
        printf("[MEMORY] Patrón escrito en bloque %lu\n", mem2->memory_id);
    }
}

void demo_filesystem() {
    printf("\n=== Demo: Sistema de Archivos ===\n");
    
    dfs_inode_t* file1 = dfs_create_file("/test.txt", 0644);
    dfs_inode_t* file2 = dfs_create_file("/data.bin", 0600);
    
    if (file1) {
        const char* content = "Contenido de prueba del archivo\n";
        dfs_write(file1, content, strlen(content), 0);
        printf("[DFS] Escrito %zu bytes en %s\n", strlen(content), file1->name);
    }
    
    if (file2) {
        uint8_t binary_data[256];
        for (int i = 0; i < 256; i++) {
            binary_data[i] = i;
        }
        dfs_write(file2, binary_data, 256, 0);
        printf("[DFS] Escrito 256 bytes binarios en %s\n", file2->name);
    }
}

void print_stats() {
    printf("\n=== Estadísticas del Sistema ===\n");
    printf("Nodo ID: %lu\n", g_kernel->node_id);
    printf("Nodos activos: %zu\n", atomic_load(&g_kernel->node_count));
    printf("Tareas en cola: %zu\n", atomic_load(&g_kernel->scheduler->task_count));
    printf("Bloques de memoria: %zu\n", atomic_load(&g_kernel->memory_manager->block_count));
    printf("Memoria total asignada: %lu bytes\n", g_kernel->memory_manager->total_allocated);
}

// ========================================
// MANEJADOR DE SEÑALES
// ========================================

void handle_sigint(int sig) {
    (void)sig; // Evitar warning de parámetro no usado
    printf("\n[SISTEMA] Apagando...\n");
    if (g_kernel) {
        g_kernel->running = false;
    }
}

// ========================================
// FUNCIÓN PRINCIPAL
// ========================================

int main(int argc, char* argv[]) {
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║     Sistema Operativo Descentralizado v0.1               ║\n");
    printf("║     Fase 2: Núcleo Funcional Distribuido                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    // Obtener ID del nodo
    node_id_t node_id = 0;
    if (argc > 1) {
        node_id = atoi(argv[1]);
    }
    
    // Inicializar kernel
    DistributedKernel64* kernel = init_kernel(node_id);
    if (!kernel) {
        fprintf(stderr, "[ERROR] No se pudo inicializar el kernel\n");
        return 1;
    }
    
    // Descubrir nodos
    discover_nodes();
    
    // Iniciar monitor de heartbeat
    pthread_create(&kernel->monitor_thread, NULL, heartbeat_monitor, kernel);
    
    // Ejecutar demos
    demo_scheduler();
    demo_memory();
    demo_filesystem();
    
    // Mostrar estadísticas
    print_stats();
    
    // Mantener el sistema en ejecución
    printf("\n[SISTEMA] Presiona Ctrl+C para salir...\n");
    
    // Configurar manejador de señales
    signal(SIGINT, handle_sigint);
    
    // Loop principal
    while (kernel->running) {
        sleep(1);
    }
    
    // Limpieza
    printf("[SISTEMA] Limpiando recursos...\n");
    
    pthread_join(kernel->monitor_thread, NULL);
    
    // TODO: Liberar toda la memoria
    
    printf("[SISTEMA] Apagado completo\n");
    
    return 0;
}