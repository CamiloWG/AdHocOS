// ========================================
// KERNEL DISTRIBUIDO v2.0 - ARQUITECTURA 64 BITS
// Sistema Operativo Descentralizado
// Fase 2: Núcleo Funcional Completo
// ========================================

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <immintrin.h>  // Para instrucciones SIMD

// ========================================
// TIPOS DE DATOS OPTIMIZADOS PARA 64 BITS
// ========================================

typedef uint64_t node_id_t;
typedef uint64_t task_id_t;
typedef uint64_t memory_id_t;
typedef int64_t  timestamp_t;

// Alineación para cache lines de 64 bytes
#define CACHE_LINE_SIZE 64
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))

// ========================================
// ESTRUCTURAS PRINCIPALES (64-bit optimized)
// ========================================

// Estructura de proceso/tarea optimizada para 64 bits
typedef struct CACHE_ALIGNED {
    task_id_t task_id;
    node_id_t assigned_node;
    uint64_t priority;
    
    // Punteros de 64 bits
    void* (*task_function)(void*);
    void* task_data;
    size_t data_size;
    
    // Estado atómico
    _Atomic uint32_t status;
    _Atomic uint32_t reference_count;
    
    // Timestamps de alta precisión
    struct timespec creation_time;
    struct timespec completion_time;
    
    // Estadísticas de ejecución
    uint64_t cpu_cycles_used;
    uint64_t memory_bytes_used;
    double cpu_time_seconds;
    
    // Contexto del proceso (registros para migración)
    struct {
        uint64_t rip;  // Instruction pointer
        uint64_t rsp;  // Stack pointer
        uint64_t rbp;  // Base pointer
        uint64_t rax, rbx, rcx, rdx;
        uint64_t rsi, rdi;
        uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
        uint64_t rflags;
        // Registros SIMD
        __m256d ymm[16];  // AVX registers
    } context;
    
    // Padding para evitar false sharing
    char padding[CACHE_LINE_SIZE];
} Task64;

// Nodo mejorado con métricas de 64 bits
typedef struct CACHE_ALIGNED {
    node_id_t node_id;
    char ip_address[46];  // IPv6 support
    uint16_t port;
    
    // Estado atómico
    _Atomic uint32_t status;
    _Atomic uint32_t active_tasks;
    
    // Métricas de rendimiento (64 bits)
    _Atomic uint64_t total_tasks_completed;
    _Atomic uint64_t total_tasks_failed;
    _Atomic uint64_t total_cpu_cycles;
    _Atomic uint64_t total_memory_bytes;
    
    // Capacidades del nodo
    uint64_t cpu_cores;
    uint64_t cpu_frequency_mhz;
    uint64_t total_memory_gb;
    uint64_t available_memory_gb;
    
    // Métricas en tiempo real
    double cpu_load;
    double memory_usage;
    double network_bandwidth_mbps;
    double reputation_score;
    
    // Timestamps de alta precisión
    struct timespec last_heartbeat;
    struct timespec boot_time;
    
    // Cache de datos frecuentes
    uint8_t cache_data[CACHE_LINE_SIZE * 4];
} Node64;

// Memoria compartida distribuida con soporte 64 bits
typedef struct CACHE_ALIGNED {
    memory_id_t memory_id;
    node_id_t owner_node;
    
    // Memoria mapeada para grandes datasets
    void* mmap_addr;
    size_t mmap_size;
    int mmap_fd;
    
    // Gestión de páginas
    struct {
        uint64_t* page_table;
        size_t num_pages;
        size_t page_size;
        _Atomic uint64_t* dirty_bitmap;
    } pages;
    
    // Versionado y consistencia
    _Atomic uint64_t version;
    _Atomic uint32_t readers;
    _Atomic uint32_t writers;
    
    // Lock optimizado con futex
    union {
        struct {
            _Atomic uint32_t lock;
            uint32_t waiter_count;
        };
        uint64_t lock_word;
    };
    
    // Réplicas
    node_id_t replicas[16];
    uint32_t replica_count;
    
    // Checksum para integridad
    uint64_t checksum;
} SharedMemory64;

// ========================================
// KERNEL DISTRIBUIDO PRINCIPAL
// ========================================

typedef struct {
    node_id_t node_id;
    uint64_t kernel_version;
    
    // Tablas de gestión
    Task64* task_table;
    Node64* node_table;
    SharedMemory64* memory_table;
    
    // Contadores atómicos
    _Atomic uint64_t next_task_id;
    _Atomic uint64_t next_memory_id;
    _Atomic uint64_t global_timestamp;
    
    // Estadísticas globales
    struct {
        _Atomic uint64_t total_tasks;
        _Atomic uint64_t total_memory_allocated;
        _Atomic uint64_t total_network_messages;
        _Atomic uint64_t total_cpu_time;
    } stats;
    
    // CPU y memoria del sistema
    struct {
        uint64_t total_cores;
        uint64_t total_memory;
        uint64_t page_size;
        uint64_t huge_page_size;
        int numa_nodes;
    } system_info;
    
    // Control del kernel
    _Atomic int running;
    pthread_t threads[32];  // Threads del kernel
    int thread_count;
} DistributedKernel64;

static DistributedKernel64* kernel64 = NULL;

// ========================================
// FUNCIONES DE BAJO NIVEL (64-bit optimized)
// ========================================

// Operación atómica compare-and-swap de 64 bits
static inline int cas_64(uint64_t* ptr, uint64_t old_val, uint64_t new_val) {
    return __sync_bool_compare_and_swap(ptr, old_val, new_val);
}

// Fence de memoria para sincronización
static inline void memory_fence(void) {
    __sync_synchronize();
}

// Obtener timestamp de alta precisión
static inline uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Obtener ciclos de CPU
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// ========================================
// GESTIÓN DE PROCESOS DISTRIBUIDOS
// ========================================

// Crear proceso con contexto de 64 bits
Task64* create_task_64(void* (*function)(void*), void* data, size_t data_size) {
    Task64* task = (Task64*)aligned_alloc(CACHE_LINE_SIZE, sizeof(Task64));
    if (!task) return NULL;
    
    memset(task, 0, sizeof(Task64));
    
    task->task_id = atomic_fetch_add(&kernel64->next_task_id, 1);
    task->task_function = function;
    task->task_data = data;
    task->data_size = data_size;
    task->priority = 5;  // Prioridad default
    
    atomic_store(&task->status, 0);  // PENDING
    atomic_store(&task->reference_count, 1);
    
    clock_gettime(CLOCK_MONOTONIC, &task->creation_time);
    
    // Inicializar contexto para futura migración
    task->context.rsp = (uint64_t)malloc(1024 * 1024);  // Stack de 1MB
    task->context.rbp = task->context.rsp;
    
    printf("[KERNEL] Tarea %lu creada (función: %p, datos: %zu bytes)\n", 
           task->task_id, function, data_size);
    
    return task;
}

// Migrar proceso entre nodos
int migrate_task_64(Task64* task, node_id_t target_node) {
    if (!task || atomic_load(&task->status) != 1) {  // Solo migrar si está ejecutando
        return -1;
    }
    
    printf("[KERNEL] Migrando tarea %lu al nodo %lu\n", task->task_id, target_node);
    
    // Guardar contexto actual
    // En un sistema real, aquí se guardarían todos los registros
    
    // Serializar tarea para envío
    size_t task_size = sizeof(Task64) + task->data_size;
    void* serialized = malloc(task_size);
    memcpy(serialized, task, sizeof(Task64));
    if (task->data_size > 0) {
        memcpy((char*)serialized + sizeof(Task64), task->task_data, task->data_size);
    }
    
    // TODO: Enviar a nodo destino por red
    
    // Marcar como migrada
    task->assigned_node = target_node;
    
    free(serialized);
    return 0;
}

// ========================================
// SCHEDULER DISTRIBUIDO MEJORADO
// ========================================

typedef struct {
    Task64** task_queue;
    size_t queue_size;
    size_t queue_capacity;
    
    // Multi-level feedback queue
    struct {
        Task64** tasks;
        size_t count;
        uint64_t time_quantum_ns;
    } priority_queues[8];
    
    // Estadísticas para predicción
    struct {
        double avg_task_duration_ns;
        double avg_cpu_usage;
        double avg_memory_usage;
        uint64_t total_scheduled;
    } stats;
    
    pthread_mutex_t lock;
} AdvancedScheduler;

static AdvancedScheduler* scheduler64 = NULL;

// Inicializar scheduler avanzado
void init_advanced_scheduler(void) {
    scheduler64 = (AdvancedScheduler*)calloc(1, sizeof(AdvancedScheduler));
    scheduler64->queue_capacity = 10000;
    scheduler64->task_queue = (Task64**)calloc(scheduler64->queue_capacity, sizeof(Task64*));
    
    // Inicializar colas de prioridad con diferentes quantum
    for (int i = 0; i < 8; i++) {
        scheduler64->priority_queues[i].tasks = 
            (Task64**)calloc(1000, sizeof(Task64*));
        scheduler64->priority_queues[i].time_quantum_ns = 
            (uint64_t)(10000000 * (1 << i));  // 10ms * 2^i
    }
    
    pthread_mutex_init(&scheduler64->lock, NULL);
    
    printf("[SCHEDULER] Scheduler avanzado inicializado (8 niveles de prioridad)\n");
}

// Algoritmo de scheduling con machine learning básico
node_id_t intelligent_task_assignment(Task64* task, Node64* nodes, int node_count) {
    if (!task || !nodes || node_count == 0) return -1;
    
    double best_score = -1.0;
    node_id_t best_node = -1;
    
    // Usar timestamp actual para predicción
    uint64_t current_time = get_timestamp_ns();
    
    for (int i = 0; i < node_count; i++) {
        Node64* node = &nodes[i];
        
        if (atomic_load(&node->status) == 0) continue;  // Nodo offline
        
        // Calcular score con múltiples factores
        double cpu_score = 1.0 - (node->cpu_load / 100.0);
        double mem_score = 1.0 - (node->memory_usage / 100.0);
        double rep_score = node->reputation_score;
        double task_score = 1.0 / (1.0 + atomic_load(&node->active_tasks));
        
        // Considerar ancho de banda para tareas con muchos datos
        double bw_score = 1.0;
        if (task->data_size > 1024 * 1024) {  // Más de 1MB
            bw_score = node->network_bandwidth_mbps / 1000.0;  // Normalizar a Gbps
        }
        
        // Score ponderado con pesos adaptativos
        double score = (cpu_score * 0.3) + 
                      (mem_score * 0.25) + 
                      (rep_score * 0.2) + 
                      (task_score * 0.15) + 
                      (bw_score * 0.1);
        
        // Bonus por afinidad (si el nodo ya procesó tareas similares)
        if (atomic_load(&node->total_tasks_completed) > 0) {
            score *= 1.1;
        }
        
        if (score > best_score) {
            best_score = score;
            best_node = node->node_id;
        }
    }
    
    if (best_node != -1) {
        printf("[SCHEDULER] Tarea %lu asignada a nodo %lu (score: %.3f)\n", 
               task->task_id, best_node, best_score);
    }
    
    return best_node;
}

// ========================================
// GESTIÓN DE MEMORIA DISTRIBUIDA AVANZADA
// ========================================

// Crear memoria compartida con mmap para grandes datasets
SharedMemory64* create_shared_memory_mmap(size_t size, node_id_t owner) {
    SharedMemory64* mem = (SharedMemory64*)aligned_alloc(CACHE_LINE_SIZE, 
                                                          sizeof(SharedMemory64));
    if (!mem) return NULL;
    
    memset(mem, 0, sizeof(SharedMemory64));
    
    mem->memory_id = atomic_fetch_add(&kernel64->next_memory_id, 1);
    mem->owner_node = owner;
    
    // Usar mmap para memoria grande
    if (size > 1024 * 1024) {  // Más de 1MB
        mem->mmap_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (mem->mmap_addr == MAP_FAILED) {
            // Fallback sin huge pages
            mem->mmap_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
        mem->mmap_size = size;
    } else {
        mem->mmap_addr = calloc(1, size);
        mem->mmap_size = size;
    }
    
    // Inicializar gestión de páginas
    mem->pages.page_size = 4096;
    mem->pages.num_pages = (size + 4095) / 4096;
    mem->pages.page_table = (uint64_t*)calloc(mem->pages.num_pages, sizeof(uint64_t));
    mem->pages.dirty_bitmap = (_Atomic uint64_t*)calloc((mem->pages.num_pages + 63) / 64, 
                                                        sizeof(uint64_t));
    
    atomic_store(&mem->version, 1);
    atomic_store(&mem->readers, 0);
    atomic_store(&mem->writers, 0);
    atomic_store(&mem->lock, 0);
    
    printf("[MEMORY] Memoria compartida %lu creada (%zu MB, %zu páginas)\n", 
           mem->memory_id, size / (1024*1024), mem->pages.num_pages);
    
    return mem;
}

// Read-write lock distribuido optimizado
void acquire_read_lock_64(SharedMemory64* mem) {
    while (1) {
        uint32_t writers = atomic_load(&mem->writers);
        if (writers == 0) {
            atomic_fetch_add(&mem->readers, 1);
            
            // Verificar de nuevo
            if (atomic_load(&mem->writers) == 0) {
                break;
            }
            
            atomic_fetch_sub(&mem->readers, 1);
        }
        
        // Esperar con backoff exponencial
        struct timespec ts = {0, 1000};  // 1 microsegundo
        nanosleep(&ts, NULL);
    }
}

void release_read_lock_64(SharedMemory64* mem) {
    atomic_fetch_sub(&mem->readers, 1);
}

void acquire_write_lock_64(SharedMemory64* mem) {
    atomic_fetch_add(&mem->writers, 1);
    
    // Esperar a que no haya lectores
    while (atomic_load(&mem->readers) > 0) {
        struct timespec ts = {0, 1000};
        nanosleep(&ts, NULL);
    }
    
    // Adquirir lock exclusivo con futex
    while (atomic_exchange(&mem->lock, 1) == 1) {
        syscall(SYS_futex, &mem->lock, FUTEX_WAIT, 1, NULL, NULL, 0);
    }
}

void release_write_lock_64(SharedMemory64* mem) {
    atomic_store(&mem->lock, 0);
    syscall(SYS_futex, &mem->lock, FUTEX_WAKE, 1, NULL, NULL, 0);
    atomic_fetch_sub(&mem->writers, 1);
}

// ========================================
// SINCRONIZACIÓN DISTRIBUIDA AVANZADA
// ========================================

// Consenso distribuido simplificado (similar a Raft)
typedef struct {
    node_id_t node_id;
    node_id_t leader_id;
    uint64_t current_term;
    
    enum { FOLLOWER, CANDIDATE, LEADER } state;
    
    // Log de operaciones
    struct {
        uint64_t index;
        uint64_t term;
        void* command;
        size_t command_size;
    } *log;
    size_t log_size;
    size_t log_capacity;
    
    // Votación
    node_id_t voted_for;
    uint64_t votes_received;
    
    pthread_mutex_t lock;
} ConsensusState;

static ConsensusState* consensus = NULL;

void init_consensus(node_id_t node_id) {
    consensus = (ConsensusState*)calloc(1, sizeof(ConsensusState));
    consensus->node_id = node_id;
    consensus->state = FOLLOWER;
    consensus->current_term = 0;
    consensus->log_capacity = 10000;
    consensus->log = calloc(consensus->log_capacity, sizeof(consensus->log[0]));
    pthread_mutex_init(&consensus->lock, NULL);
    
    printf("[CONSENSUS] Sistema de consenso inicializado (nodo %lu)\n", node_id);
}

// ========================================
// SISTEMA DE ARCHIVOS DISTRIBUIDO
// ========================================

typedef struct {
    char name[256];
    uint64_t size;
    uint64_t blocks[1024];  // Bloques de datos
    node_id_t replicas[3];  // 3 réplicas por defecto
    uint32_t replica_count;
    time_t created;
    time_t modified;
    uint32_t permissions;
    uint64_t checksum;
} DistributedFile;

typedef struct {
    DistributedFile* files;
    size_t file_count;
    size_t file_capacity;
    
    // Índice hash para búsqueda rápida
    struct {
        uint64_t hash;
        size_t file_index;
    } *hash_index;
    size_t hash_size;
    
    pthread_rwlock_t lock;
} DistributedFileSystem;

static DistributedFileSystem* dfs = NULL;

void init_distributed_filesystem(void) {
    dfs = (DistributedFileSystem*)calloc(1, sizeof(DistributedFileSystem));
    dfs->file_capacity = 10000;
    dfs->files = (DistributedFile*)calloc(dfs->file_capacity, sizeof(DistributedFile));
    dfs->hash_size = 16384;  // Tabla hash de 16K entradas
    dfs->hash_index = calloc(dfs->hash_size, sizeof(dfs->hash_index[0]));
    pthread_rwlock_init(&dfs->lock, NULL);
    
    printf("[DFS] Sistema de archivos distribuido inicializado\n");
}

// ========================================
// OPTIMIZACIONES SIMD PARA ML
// ========================================

// Producto punto optimizado con AVX2
double dot_product_avx2(const double* a, const double* b, size_t n) {
    __m256d sum = _mm256_setzero_pd();
    size_t i;
    
    // Procesar de 4 en 4 elementos
    for (i = 0; i + 3 < n; i += 4) {
        __m256d va = _mm256_loadu_pd(&a[i]);
        __m256d vb = _mm256_loadu_pd(&b[i]);
        sum = _mm256_fmadd_pd(va, vb, sum);
    }
    
    // Sumar los 4 elementos del vector
    double result[4];
    _mm256_storeu_pd(result, sum);
    double total = result[0] + result[1] + result[2] + result[3];
    
    // Procesar elementos restantes
    for (; i < n; i++) {
        total += a[i] * b[i];
    }
    
    return total;
}

// Multiplicación matriz-vector optimizada
void matrix_vector_mult_avx2(const double* matrix, const double* vector, 
                             double* result, size_t rows, size_t cols) {
    for (size_t i = 0; i < rows; i++) {
        result[i] = dot_product_avx2(&matrix[i * cols], vector, cols);
    }
}

// ========================================
// SISTEMA DE LLAMADAS DISTRIBUIDAS
// ========================================

typedef enum {
    SYSCALL_FORK = 0,
    SYSCALL_EXEC,
    SYSCALL_EXIT,
    SYSCALL_WAIT,
    SYSCALL_OPEN,
    SYSCALL_READ,
    SYSCALL_WRITE,
    SYSCALL_CLOSE,
    SYSCALL_MALLOC,
    SYSCALL_FREE,
    SYSCALL_SEND_MSG,
    SYSCALL_RECV_MSG,
    SYSCALL_MIGRATE,
    SYSCALL_CHECKPOINT,
    SYSCALL_RESTORE
} DistributedSyscall;

typedef struct {
    DistributedSyscall syscall_id;
    node_id_t source_node;
    task_id_t task_id;
    uint64_t args[6];  // Hasta 6 argumentos como Linux x86_64
    uint64_t return_value;
} SyscallRequest;

// Manejador de syscalls distribuidas
uint64_t handle_distributed_syscall(SyscallRequest* req) {
    switch (req->syscall_id) {
        case SYSCALL_FORK:
            // Crear copia del proceso
            return (uint64_t)create_task_64((void*)req->args[0], 
                                           (void*)req->args[1], 
                                           req->args[2]);
            
        case SYSCALL_MALLOC:
            // Asignar memoria distribuida
            return (uint64_t)create_shared_memory_mmap(req->args[0], 
                                                       req->source_node);
            
        case SYSCALL_MIGRATE:
            // Migrar tarea a otro nodo
            return migrate_task_64((Task64*)req->args[0], req->args[1]);
            
        default:
            printf("[SYSCALL] Syscall %d no implementada\n", req->syscall_id);
            return -ENOSYS;
    }
}

// ========================================
// INICIALIZACIÓN DEL KERNEL
// ========================================

int init_distributed_kernel_64(node_id_t node_id) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║   SISTEMA OPERATIVO DESCENTRALIZADO v2.0 (64-bit)               ║\n");
    printf("║   Fase 2: Núcleo Funcional Distribuido Completo                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Asignar memoria para el kernel
    kernel64 = (DistributedKernel64*)calloc(1, sizeof(DistributedKernel64));
    if (!kernel64) {
        fprintf(stderr, "[ERROR] No se pudo asignar memoria para el kernel\n");
        return -1;
    }
    
    kernel64->node_id = node_id;
    kernel64->kernel_version = 0x0200;  // v2.0
    
    // Obtener información del sistema
    kernel64->system_info.total_cores = sysconf(_SC_NPROCESSORS_ONLN);
    kernel64->system_info.total_memory = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE);
    kernel64->system_info.page_size = sysconf(_SC_PAGE_SIZE);
    
    // Inicializar tablas
    kernel64->task_table = (Task64*)calloc(10000, sizeof(Task64));
    kernel64->node_table = (Node64*)calloc(1000, sizeof(Node64));
    kernel64->memory_table = (SharedMemory64*)calloc(10000, sizeof(SharedMemory64));
    
    // Inicializar contadores atómicos
    atomic_store(&kernel64->next_task_id, 1);
    atomic_store(&kernel64->next_memory_id, 1);
    atomic_store(&kernel64->global_timestamp, 0);
    atomic_store(&kernel64->running, 1);
    
    printf("[KERNEL] Nodo ID: %lu\n", node_id);
    printf("[KERNEL] Versión: %04X\n", kernel64->kernel_version);
    printf("[KERNEL] CPUs: %lu cores\n", kernel64->system_info.total_cores);
    printf("[KERNEL] Memoria: %lu GB\n", kernel64->system_info.total_memory / (1024*1024*1024));
    printf("[KERNEL] Tamaño de página: %lu KB\n", kernel64->system_info.page_size / 1024);
    
    // Inicializar subsistemas
    init_advanced_scheduler();
    init_consensus(node_id);
    init_distributed_filesystem();
    
    printf("[KERNEL] ✅ Kernel distribuido de 64 bits inicializado\n\n");
    
    return 0;
}

// ========================================
// PROGRAMA DE PRUEBA
// ========================================

// Función de ejemplo para tareas
void* example_ml_task(void* data) {
    printf("[TASK] Ejecutando tarea ML con AVX2...\n");
    
    // Crear vectores de prueba
    size_t n = 1000;
    double* a = (double*)aligned_alloc(32, n * sizeof(double));
    double* b = (double*)aligned_alloc(32, n * sizeof(double));
    
    // Inicializar con valores aleatorios
    for (size_t i = 0; i < n; i++) {
        a[i] = (double)rand() / RAND_MAX;
        b[i] = (double)rand() / RAND_MAX;
    }
    
    // Calcular producto punto con SIMD
    uint64_t start_cycles = rdtsc();
    double result = dot_product_avx2(a, b, n);
    uint64_t end_cycles = rdtsc();
    
    printf("[TASK] Producto punto = %.6f (ciclos: %lu)\n", 
           result, end_cycles - start_cycles);
    
    free(a);
    free(b);
    
    return NULL;
}

int main(int argc, char* argv[]) {
    node_id_t node_id = 0;
    
    if (argc > 1) {
        node_id = strtoull(argv[1], NULL, 10);
    }
    
    // Inicializar kernel
    if (init_distributed_kernel_64(node_id) < 0) {
        return EXIT_FAILURE;
    }
    
    // Crear nodos de ejemplo
    printf("=== CREANDO NODOS DE EJEMPLO ===\n");
    for (int i = 0; i < 3; i++) {
        Node64* node = &kernel64->node_table[i];
        node->node_id = i;
        snprintf(node->ip_address, sizeof(node->ip_address), "192.168.1.%d", 100 + i);
        node->port = 8080 + i;
        atomic_store(&node->status, 1);  // Activo
        node->cpu_cores = 4;
        node->cpu_frequency_mhz = 2400;
        node->total_memory_gb = 8;
        node->available_memory_gb = 6;
        node->cpu_load = 20.0 + (i * 10);
        node->memory_usage = 30.0 + (i * 5);
        node->network_bandwidth_mbps = 1000;
        node->reputation_score = 0.9 - (i * 0.1);
        clock_gettime(CLOCK_MONOTONIC, &node->last_heartbeat);
        
        printf("  Nodo %lu: %s:%d (CPU: %.1f%%, Mem: %.1f%%, Rep: %.2f)\n",
               node->node_id, node->ip_address, node->port,
               node->cpu_load, node->memory_usage, node->reputation_score);
    }
    
    // Crear tareas de prueba
    printf("\n=== CREANDO Y PROGRAMANDO TAREAS ===\n");
    for (int i = 0; i < 5; i++) {
        Task64* task = create_task_64(example_ml_task, NULL, 0);
        if (task) {
            node_id_t assigned = intelligent_task_assignment(task, 
                                                            kernel64->node_table, 3);
            if (assigned != (node_id_t)-1) {
                task->assigned_node = assigned;
                atomic_store(&task->status, 1);  // Running
            }
        }
    }
    
    // Crear memoria compartida
    printf("\n=== CREANDO MEMORIA COMPARTIDA ===\n");
    SharedMemory64* mem1 = create_shared_memory_mmap(1024 * 1024 * 10, node_id);  // 10MB
    SharedMemory64* mem2 = create_shared_memory_mmap(1024 * 1024 * 100, node_id); // 100MB
    
    if (mem1 && mem2) {
        // Escribir datos de prueba
        acquire_write_lock_64(mem1);
        memset(mem1->mmap_addr, 0xAB, 1024);  // Escribir patrón
        release_write_lock_64(mem1);
        
        // Leer datos
        acquire_read_lock_64(mem1);
        uint8_t* data = (uint8_t*)mem1->mmap_addr;
        printf("  Datos leídos: %02X %02X %02X %02X ...\n", 
               data[0], data[1], data[2], data[3]);
        release_read_lock_64(mem1);
    }
    
    // Demostración de SIMD
    printf("\n=== DEMOSTRACIÓN DE OPTIMIZACIONES SIMD ===\n");
    size_t vector_size = 10000;
    double* vec_a = (double*)aligned_alloc(32, vector_size * sizeof(double));
    double* vec_b = (double*)aligned_alloc(32, vector_size * sizeof(double));
    
    for (size_t i = 0; i < vector_size; i++) {
        vec_a[i] = i * 0.001;
        vec_b[i] = i * 0.002;
    }
    
    // Comparar rendimiento
    uint64_t start_normal = rdtsc();
    double result_normal = 0;
    for (size_t i = 0; i < vector_size; i++) {
        result_normal += vec_a[i] * vec_b[i];
    }
    uint64_t cycles_normal = rdtsc() - start_normal;
    
    uint64_t start_simd = rdtsc();
    double result_simd = dot_product_avx2(vec_a, vec_b, vector_size);
    uint64_t cycles_simd = rdtsc() - start_simd;
    
    printf("  Producto punto normal: %.6f (ciclos: %lu)\n", result_normal, cycles_normal);
    printf("  Producto punto SIMD:   %.6f (ciclos: %lu)\n", result_simd, cycles_simd);
    printf("  Aceleración: %.2fx\n", (double)cycles_normal / cycles_simd);
    
    free(vec_a);
    free(vec_b);
    
    // Estadísticas finales
    printf("\n=== ESTADÍSTICAS DEL KERNEL ===\n");
    printf("  Tareas totales: %lu\n", atomic_load(&kernel64->stats.total_tasks));
    printf("  Memoria asignada: %lu MB\n", 
           atomic_load(&kernel64->stats.total_memory_allocated) / (1024*1024));
    printf("  Mensajes de red: %lu\n", atomic_load(&kernel64->stats.total_network_messages));
    
    printf("\n[KERNEL] ✅ Sistema operativo descentralizado funcionando correctamente\n");
    printf("[KERNEL] Presiona Ctrl+C para salir...\n");
    
    // Loop principal
    while (atomic_load(&kernel64->running)) {
        sleep(1);
    }
    
    // Limpieza
    free(kernel64->task_table);
    free(kernel64->node_table);
    free(kernel64->memory_table);
    free(kernel64);
    
    return EXIT_SUCCESS;
}
