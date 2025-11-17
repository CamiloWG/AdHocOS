#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "../common.h"

// ========================================
// ESTRUCTURAS DEL GESTOR DE MEMORIA
// ========================================

typedef struct {
    SharedMemory* memory_blocks[MAX_MEMORY_BLOCKS];
    int block_count;
    int next_memory_id;
    pthread_mutex_t memory_lock;
} DistributedMemoryManager;

// ========================================
// FUNCIONES PÚBLICAS
// ========================================

// Inicialización
void init_memory_manager();
void cleanup_memory_manager();

// Gestión de memoria
SharedMemory* allocate_shared_memory(size_t size, int owner_node);
int free_shared_memory(int memory_id);
SharedMemory* get_shared_memory(int memory_id);

// Operaciones de lectura/escritura
int write_shared_memory(SharedMemory* mem, void* data, size_t size, size_t offset);
int read_shared_memory(SharedMemory* mem, void* buffer, size_t size, size_t offset);

// Replicación
int replicate_memory(SharedMemory* mem, int target_node);
void sync_memory_replicas(SharedMemory* mem);

// Estadísticas
void print_memory_stats();

// Variable global
extern DistributedMemoryManager* memory_manager;

#endif // MEMORY_MANAGER_H