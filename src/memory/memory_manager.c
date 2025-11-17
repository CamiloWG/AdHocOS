#include "../common.h"
#include "memory_manager.h"

// ========================================
// GESTOR DE MEMORIA DISTRIBUIDA
// ========================================

DistributedMemoryManager* memory_manager = NULL;

void init_memory_manager() {
    memory_manager = (DistributedMemoryManager*)malloc(sizeof(DistributedMemoryManager));
    memory_manager->block_count = 0;
    memory_manager->next_memory_id = 1;
    pthread_mutex_init(&memory_manager->memory_lock, NULL);
    
    log_info("Gestor de memoria distribuida inicializado");
}

SharedMemory* allocate_shared_memory(size_t size, int owner_node) {
    pthread_mutex_lock(&memory_manager->memory_lock);
    
    if (memory_manager->block_count >= MAX_MEMORY_BLOCKS) {
        log_error("No hay espacio para más bloques de memoria");
        pthread_mutex_unlock(&memory_manager->memory_lock);
        return NULL;
    }
    
    SharedMemory* mem = (SharedMemory*)malloc(sizeof(SharedMemory));
    mem->data = malloc(size);
    mem->size = size;
    mem->owner_node = owner_node;
    mem->reference_count = 1;
    mem->memory_id = memory_manager->next_memory_id++;
    mem->replication_count = 0;
    pthread_mutex_init(&mem->lock, NULL);
    
    // Inicializar datos a cero
    memset(mem->data, 0, size);
    memset(mem->replicated_nodes, -1, sizeof(mem->replicated_nodes));
    
    memory_manager->memory_blocks[memory_manager->block_count++] = mem;
    
    log_info("Memoria asignada: ID=%d, Tamaño=%zu bytes, Propietario=Nodo %d", 
             mem->memory_id, size, owner_node);
    
    pthread_mutex_unlock(&memory_manager->memory_lock);
    return mem;
}

int free_shared_memory(int memory_id) {
    pthread_mutex_lock(&memory_manager->memory_lock);
    
    for (int i = 0; i < memory_manager->block_count; i++) {
        SharedMemory* mem = memory_manager->memory_blocks[i];
        if (mem->memory_id == memory_id) {
            mem->reference_count--;
            
            if (mem->reference_count <= 0) {
                log_info("Liberando memoria ID=%d", memory_id);
                pthread_mutex_destroy(&mem->lock);
                free(mem->data);
                free(mem);
                
                // Reorganizar array
                for (int j = i; j < memory_manager->block_count - 1; j++) {
                    memory_manager->memory_blocks[j] = memory_manager->memory_blocks[j + 1];
                }
                memory_manager->block_count--;
            }
            
            pthread_mutex_unlock(&memory_manager->memory_lock);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&memory_manager->memory_lock);
    return -1;
}

SharedMemory* get_shared_memory(int memory_id) {
    pthread_mutex_lock(&memory_manager->memory_lock);
    
    for (int i = 0; i < memory_manager->block_count; i++) {
        if (memory_manager->memory_blocks[i]->memory_id == memory_id) {
            SharedMemory* mem = memory_manager->memory_blocks[i];
            pthread_mutex_unlock(&memory_manager->memory_lock);
            return mem;
        }
    }
    
    pthread_mutex_unlock(&memory_manager->memory_lock);
    return NULL;
}

int write_shared_memory(SharedMemory* mem, void* data, size_t size, size_t offset) {
    if (!mem || !data) return -1;
    if (offset + size > mem->size) return -1;
    
    pthread_mutex_lock(&mem->lock);
    memcpy((char*)mem->data + offset, data, size);
    pthread_mutex_unlock(&mem->lock);
    
    log_debug("Escritura en memoria %d: %zu bytes en offset %zu", 
              mem->memory_id, size, offset);
    return 0;
}

int read_shared_memory(SharedMemory* mem, void* buffer, size_t size, size_t offset) {
    if (!mem || !buffer) return -1;
    if (offset + size > mem->size) return -1;
    
    pthread_mutex_lock(&mem->lock);
    memcpy(buffer, (char*)mem->data + offset, size);
    pthread_mutex_unlock(&mem->lock);
    
    log_debug("Lectura de memoria %d: %zu bytes desde offset %zu", 
              mem->memory_id, size, offset);
    return 0;
}

int replicate_memory(SharedMemory* mem, int target_node) {
    if (!mem) return -1;
    
    pthread_mutex_lock(&mem->lock);
    
    // Verificar si ya está replicado en ese nodo
    for (int i = 0; i < mem->replication_count; i++) {
        if (mem->replicated_nodes[i] == target_node) {
            pthread_mutex_unlock(&mem->lock);
            return 0; // Ya replicado
        }
    }
    
    // Agregar nodo a lista de réplicas
    if (mem->replication_count < MAX_NODES) {
        mem->replicated_nodes[mem->replication_count++] = target_node;
        mem->reference_count++;
        
        log_info("Memoria %d replicada al nodo %d (réplicas: %d)", 
                 mem->memory_id, target_node, mem->replication_count);
    }
    
    pthread_mutex_unlock(&mem->lock);
    return 0;
}

void sync_memory_replicas(SharedMemory* mem) {
    if (!mem || mem->replication_count == 0) return;
    
    pthread_mutex_lock(&mem->lock);
    
    log_info("Sincronizando memoria %d con %d réplicas", 
             mem->memory_id, mem->replication_count);
    
    // En una implementación real, enviaríamos los datos a cada réplica
    for (int i = 0; i < mem->replication_count; i++) {
        log_debug("  -> Sincronizando con nodo %d", mem->replicated_nodes[i]);
    }
    
    pthread_mutex_unlock(&mem->lock);
}

void print_memory_stats() {
    pthread_mutex_lock(&memory_manager->memory_lock);
    
    size_t total_allocated = 0;
    int replicated_blocks = 0;
    
    for (int i = 0; i < memory_manager->block_count; i++) {
        SharedMemory* mem = memory_manager->memory_blocks[i];
        total_allocated += mem->size;
        if (mem->replication_count > 0) {
            replicated_blocks++;
        }
    }
    
    log_info("📊 Estadísticas de Memoria:");
    log_info("   Bloques: %d | Total: %zu KB | Replicados: %d",
             memory_manager->block_count, total_allocated / 1024, replicated_blocks);
    
    pthread_mutex_unlock(&memory_manager->memory_lock);
}

void cleanup_memory_manager() {
    if (memory_manager) {
        pthread_mutex_lock(&memory_manager->memory_lock);
        
        // Liberar todos los bloques
        for (int i = 0; i < memory_manager->block_count; i++) {
            SharedMemory* mem = memory_manager->memory_blocks[i];
            pthread_mutex_destroy(&mem->lock);
            free(mem->data);
            free(mem);
        }
        
        pthread_mutex_unlock(&memory_manager->memory_lock);
        pthread_mutex_destroy(&memory_manager->memory_lock);
        free(memory_manager);
        memory_manager = NULL;
    }
}