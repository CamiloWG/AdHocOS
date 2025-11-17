// ========================================
// SISTEMA DE ARCHIVOS DISTRIBUIDO (DFS)
// Para Sistema Operativo Descentralizado 64-bit
// ========================================

#ifndef DFS_H
#define DFS_H

#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
#include <openssl/sha.h>

// ========================================
// CONSTANTES Y CONFIGURACIÓN
// ========================================

#define DFS_BLOCK_SIZE      4096        // 4KB por bloque
#define DFS_MAX_FILE_SIZE   (1ULL << 40) // 1TB máximo por archivo
#define DFS_MAX_NAME_LEN    256
#define DFS_MAX_PATH_LEN    4096
#define DFS_REPLICATION_FACTOR 3
#define DFS_STRIPE_SIZE     (64 * 1024)  // 64KB stripe para RAID-like

// Tipos de archivo
typedef enum {
    DFS_TYPE_FILE = 1,
    DFS_TYPE_DIRECTORY = 2,
    DFS_TYPE_SYMLINK = 3,
    DFS_TYPE_DEVICE = 4,
    DFS_TYPE_PIPE = 5
} dfs_file_type_t;

// Permisos (similar a UNIX)
#define DFS_PERM_READ   0x04
#define DFS_PERM_WRITE  0x02
#define DFS_PERM_EXEC   0x01

// ========================================
// ESTRUCTURAS DE DATOS
// ========================================

// Bloque de datos
typedef struct {
    uint64_t block_id;
    uint64_t file_id;
    uint64_t offset;          // Offset dentro del archivo
    uint8_t data[DFS_BLOCK_SIZE];
    uint64_t checksum;        // CRC64 o SHA256
    node_id_t primary_node;   // Nodo primario
    node_id_t replicas[DFS_REPLICATION_FACTOR];
    _Atomic uint32_t version;
    _Atomic uint32_t ref_count;
} dfs_block_t;

// Metadata de archivo
typedef struct {
    uint64_t inode;
    char name[DFS_MAX_NAME_LEN];
    dfs_file_type_t type;
    uint64_t size;
    uint64_t blocks_count;
    uint64_t* block_list;     // Lista de IDs de bloques
    
    // Permisos y propietario
    uint32_t uid;
    uint32_t gid;
    uint16_t permissions;
    
    // Timestamps
    struct timespec created;
    struct timespec modified;
    struct timespec accessed;
    
    // Replicación y distribución
    node_id_t preferred_nodes[DFS_REPLICATION_FACTOR];
    uint32_t stripe_width;    // Para distribución tipo RAID
    
    // Cache y optimización
    _Atomic uint64_t cache_hits;
    _Atomic uint64_t access_count;
    
    // Lock para consistencia
    pthread_rwlock_t lock;
} dfs_file_t;

// Directorio
typedef struct dfs_directory {
    uint64_t inode;
    char name[DFS_MAX_NAME_LEN];
    struct dfs_directory* parent;
    
    // Entradas del directorio
    struct {
        char name[DFS_MAX_NAME_LEN];
        uint64_t inode;
        dfs_file_type_t type;
    } *entries;
    size_t entry_count;
    size_t entry_capacity;
    
    // Metadata
    uint32_t uid;
    uint32_t gid;
    uint16_t permissions;
    struct timespec created;
    struct timespec modified;
    
    pthread_rwlock_t lock;
} dfs_directory_t;

// Cache distribuido
typedef struct {
    struct {
        uint64_t block_id;
        void* data;
        size_t size;
        time_t timestamp;
        _Atomic uint32_t hits;
    } *entries;
    size_t capacity;
    size_t count;
    
    // LRU management
    struct {
        uint64_t block_id;
        struct lru_node* next;
        struct lru_node* prev;
    } *lru_list;
    
    pthread_mutex_t lock;
} dfs_cache_t;

// Sistema de archivos global
typedef struct {
    // Raíz del sistema de archivos
    dfs_directory_t* root;
    
    // Tablas de inodos
    dfs_file_t** inode_table;
    size_t inode_count;
    size_t inode_capacity;
    _Atomic uint64_t next_inode;
    
    // Tabla de bloques
    dfs_block_t** block_table;
    size_t block_count;
    size_t block_capacity;
    _Atomic uint64_t next_block_id;
    
    // Cache
    dfs_cache_t* cache;
    
    // Estadísticas
    struct {
        _Atomic uint64_t total_files;
        _Atomic uint64_t total_directories;
        _Atomic uint64_t total_blocks;
        _Atomic uint64_t total_bytes;
        _Atomic uint64_t cache_hits;
        _Atomic uint64_t cache_misses;
    } stats;
    
    // Configuración
    struct {
        size_t max_file_size;
        size_t block_size;
        uint32_t replication_factor;
        uint32_t stripe_width;
        bool enable_compression;
        bool enable_encryption;
    } config;
    
    pthread_rwlock_t global_lock;
} dfs_t;

// ========================================
// IMPLEMENTACIÓN
// ========================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <zlib.h>  // Para compresión

static dfs_t* g_dfs = NULL;

// Hash function para nombres
static uint64_t hash_string(const char* str) {
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Calcular checksum de un bloque
static uint64_t calculate_checksum(const void* data, size_t size) {
    // Usar CRC64 o SHA256 para integridad
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data, size);
    SHA256_Final(hash, &sha256);
    
    // Convertir a uint64_t
    uint64_t checksum = 0;
    for (int i = 0; i < 8; i++) {
        checksum = (checksum << 8) | hash[i];
    }
    return checksum;
}

// Comprimir datos si está habilitado
static size_t compress_block(const void* src, size_t src_size, void* dst, size_t dst_size) {
    if (!g_dfs->config.enable_compression) {
        memcpy(dst, src, src_size);
        return src_size;
    }
    
    uLongf compressed_size = dst_size;
    int result = compress2((Bytef*)dst, &compressed_size, 
                          (const Bytef*)src, src_size, 
                          Z_BEST_SPEED);
    
    if (result != Z_OK) {
        memcpy(dst, src, src_size);
        return src_size;
    }
    
    return compressed_size;
}

// ========================================
// FUNCIONES DE INICIALIZACIÓN
// ========================================

dfs_t* dfs_init(size_t cache_size_mb) {
    g_dfs = (dfs_t*)calloc(1, sizeof(dfs_t));
    if (!g_dfs) return NULL;
    
    // Configuración por defecto
    g_dfs->config.max_file_size = DFS_MAX_FILE_SIZE;
    g_dfs->config.block_size = DFS_BLOCK_SIZE;
    g_dfs->config.replication_factor = DFS_REPLICATION_FACTOR;
    g_dfs->config.stripe_width = 4;  // 4 nodos para striping
    g_dfs->config.enable_compression = true;
    g_dfs->config.enable_encryption = false;
    
    // Inicializar tablas
    g_dfs->inode_capacity = 10000;
    g_dfs->inode_table = (dfs_file_t**)calloc(g_dfs->inode_capacity, sizeof(dfs_file_t*));
    atomic_store(&g_dfs->next_inode, 1);
    
    g_dfs->block_capacity = 100000;
    g_dfs->block_table = (dfs_block_t**)calloc(g_dfs->block_capacity, sizeof(dfs_block_t*));
    atomic_store(&g_dfs->next_block_id, 1);
    
    // Crear directorio raíz
    g_dfs->root = (dfs_directory_t*)calloc(1, sizeof(dfs_directory_t));
    g_dfs->root->inode = atomic_fetch_add(&g_dfs->next_inode, 1);
    strcpy(g_dfs->root->name, "/");
    g_dfs->root->parent = NULL;
    g_dfs->root->entry_capacity = 100;
    g_dfs->root->entries = calloc(g_dfs->root->entry_capacity, 
                                  sizeof(g_dfs->root->entries[0]));
    g_dfs->root->permissions = 0755;
    clock_gettime(CLOCK_REALTIME, &g_dfs->root->created);
    pthread_rwlock_init(&g_dfs->root->lock, NULL);
    
    // Inicializar cache
    size_t cache_entries = (cache_size_mb * 1024 * 1024) / DFS_BLOCK_SIZE;
    g_dfs->cache = (dfs_cache_t*)calloc(1, sizeof(dfs_cache_t));
    g_dfs->cache->capacity = cache_entries;
    g_dfs->cache->entries = calloc(cache_entries, sizeof(g_dfs->cache->entries[0]));
    pthread_mutex_init(&g_dfs->cache->lock, NULL);
    
    pthread_rwlock_init(&g_dfs->global_lock, NULL);
    
    printf("[DFS] Sistema de archivos distribuido inicializado\n");
    printf("[DFS] Cache: %zu MB (%zu bloques)\n", cache_size_mb, cache_entries);
    printf("[DFS] Replicación: %d copias\n", g_dfs->config.replication_factor);
    printf("[DFS] Compresión: %s\n", g_dfs->config.enable_compression ? "Habilitada" : "Deshabilitada");
    
    return g_dfs;
}

// ========================================
// OPERACIONES DE ARCHIVO
// ========================================

// Crear archivo
dfs_file_t* dfs_create_file(const char* path, uint16_t permissions) {
    pthread_rwlock_wrlock(&g_dfs->global_lock);
    
    // Asignar nuevo inodo
    uint64_t inode = atomic_fetch_add(&g_dfs->next_inode, 1);
    
    dfs_file_t* file = (dfs_file_t*)calloc(1, sizeof(dfs_file_t));
    file->inode = inode;
    
    // Extraer nombre del path
    const char* name = strrchr(path, '/');
    if (name) {
        strncpy(file->name, name + 1, DFS_MAX_NAME_LEN - 1);
    } else {
        strncpy(file->name, path, DFS_MAX_NAME_LEN - 1);
    }
    
    file->type = DFS_TYPE_FILE;
    file->size = 0;
    file->blocks_count = 0;
    file->block_list = NULL;
    file->permissions = permissions;
    file->uid = getuid();
    file->gid = getgid();
    
    clock_gettime(CLOCK_REALTIME, &file->created);
    file->modified = file->created;
    file->accessed = file->created;
    
    pthread_rwlock_init(&file->lock, NULL);
    
    // Agregar a tabla de inodos
    if (g_dfs->inode_count < g_dfs->inode_capacity) {
        g_dfs->inode_table[g_dfs->inode_count++] = file;
    }
    
    atomic_fetch_add(&g_dfs->stats.total_files, 1);
    
    pthread_rwlock_unlock(&g_dfs->global_lock);
    
    printf("[DFS] Archivo creado: %s (inode: %lu)\n", file->name, inode);
    
    return file;
}

// Escribir datos en archivo
ssize_t dfs_write(dfs_file_t* file, const void* buffer, size_t size, off_t offset) {
    if (!file || !buffer) return -EINVAL;
    
    pthread_rwlock_wrlock(&file->lock);
    
    // Calcular bloques necesarios
    size_t start_block = offset / DFS_BLOCK_SIZE;
    size_t end_block = (offset + size - 1) / DFS_BLOCK_SIZE;
    size_t blocks_needed = end_block - start_block + 1;
    
    // Expandir lista de bloques si es necesario
    if (end_block >= file->blocks_count) {
        uint64_t* new_list = (uint64_t*)realloc(file->block_list, 
                                                (end_block + 1) * sizeof(uint64_t));
        if (!new_list) {
            pthread_rwlock_unlock(&file->lock);
            return -ENOMEM;
        }
        
        // Asignar nuevos bloques
        for (size_t i = file->blocks_count; i <= end_block; i++) {
            new_list[i] = atomic_fetch_add(&g_dfs->next_block_id, 1);
            
            // Crear bloque
            dfs_block_t* block = (dfs_block_t*)calloc(1, sizeof(dfs_block_t));
            block->block_id = new_list[i];
            block->file_id = file->inode;
            block->offset = i * DFS_BLOCK_SIZE;
            atomic_store(&block->version, 1);
            atomic_store(&block->ref_count, 1);
            
            // Agregar a tabla de bloques
            if (g_dfs->block_count < g_dfs->block_capacity) {
                g_dfs->block_table[g_dfs->block_count++] = block;
            }
        }
        
        file->block_list = new_list;
        file->blocks_count = end_block + 1;
    }
    
    // Escribir datos en los bloques
    size_t bytes_written = 0;
    const uint8_t* data = (const uint8_t*)buffer;
    
    for (size_t i = start_block; i <= end_block; i++) {
        // Buscar bloque
        dfs_block_t* block = NULL;
        for (size_t j = 0; j < g_dfs->block_count; j++) {
            if (g_dfs->block_table[j]->block_id == file->block_list[i]) {
                block = g_dfs->block_table[j];
                break;
            }
        }
        
        if (!block) continue;
        
        // Calcular offset y tamaño dentro del bloque
        size_t block_offset = (i == start_block) ? (offset % DFS_BLOCK_SIZE) : 0;
        size_t block_size = DFS_BLOCK_SIZE - block_offset;
        if (i == end_block) {
            block_size = ((offset + size) % DFS_BLOCK_SIZE) - block_offset;
            if (block_size == 0) block_size = DFS_BLOCK_SIZE - block_offset;
        }
        
        // Escribir datos
        memcpy(block->data + block_offset, data + bytes_written, block_size);
        bytes_written += block_size;
        
        // Actualizar checksum y versión
        block->checksum = calculate_checksum(block->data, DFS_BLOCK_SIZE);
        atomic_fetch_add(&block->version, 1);
    }
    
    // Actualizar metadata del archivo
    if (offset + size > file->size) {
        file->size = offset + size;
    }
    clock_gettime(CLOCK_REALTIME, &file->modified);
    
    atomic_fetch_add(&g_dfs->stats.total_bytes, bytes_written);
    
    pthread_rwlock_unlock(&file->lock);
    
    return bytes_written;
}

// Leer datos de archivo
ssize_t dfs_read(dfs_file_t* file, void* buffer, size_t size, off_t offset) {
    if (!file || !buffer) return -EINVAL;
    if (offset >= file->size) return 0;
    
    pthread_rwlock_rdlock(&file->lock);
    
    // Ajustar tamaño si excede el archivo
    if (offset + size > file->size) {
        size = file->size - offset;
    }
    
    // Calcular bloques a leer
    size_t start_block = offset / DFS_BLOCK_SIZE;
    size_t end_block = (offset + size - 1) / DFS_BLOCK_SIZE;
    
    size_t bytes_read = 0;
    uint8_t* data = (uint8_t*)buffer;
    
    for (size_t i = start_block; i <= end_block && i < file->blocks_count; i++) {
        // Buscar en cache primero
        dfs_block_t* block = NULL;
        
        pthread_mutex_lock(&g_dfs->cache->lock);
        for (size_t j = 0; j < g_dfs->cache->count; j++) {
            if (g_dfs->cache->entries[j].block_id == file->block_list[i]) {
                // Cache hit
                atomic_fetch_add(&g_dfs->cache->entries[j].hits, 1);
                atomic_fetch_add(&g_dfs->stats.cache_hits, 1);
                
                // Copiar desde cache
                size_t block_offset = (i == start_block) ? (offset % DFS_BLOCK_SIZE) : 0;
                size_t block_size = DFS_BLOCK_SIZE - block_offset;
                if (i == end_block) {
                    block_size = ((offset + size) % DFS_BLOCK_SIZE) - block_offset;
                    if (block_size == 0) block_size = DFS_BLOCK_SIZE - block_offset;
                }
                
                memcpy(data + bytes_read, 
                      (uint8_t*)g_dfs->cache->entries[j].data + block_offset, 
                      block_size);
                bytes_read += block_size;
                
                pthread_mutex_unlock(&g_dfs->cache->lock);
                continue;
            }
        }
        pthread_mutex_unlock(&g_dfs->cache->lock);
        
        // Cache miss - buscar en tabla de bloques
        atomic_fetch_add(&g_dfs->stats.cache_misses, 1);
        
        for (size_t j = 0; j < g_dfs->block_count; j++) {
            if (g_dfs->block_table[j]->block_id == file->block_list[i]) {
                block = g_dfs->block_table[j];
                break;
            }
        }
        
        if (!block) continue;
        
        // Verificar checksum
        uint64_t checksum = calculate_checksum(block->data, DFS_BLOCK_SIZE);
        if (checksum != block->checksum) {
            printf("[DFS] ⚠️  Checksum incorrecto en bloque %lu\n", block->block_id);
            // Intentar recuperar desde réplica
            continue;
        }
        
        // Calcular offset y tamaño dentro del bloque
        size_t block_offset = (i == start_block) ? (offset % DFS_BLOCK_SIZE) : 0;
        size_t block_size = DFS_BLOCK_SIZE - block_offset;
        if (i == end_block) {
            block_size = ((offset + size) % DFS_BLOCK_SIZE) - block_offset;
            if (block_size == 0) block_size = DFS_BLOCK_SIZE - block_offset;
        }
        
        // Leer datos
        memcpy(data + bytes_read, block->data + block_offset, block_size);
        bytes_read += block_size;
        
        // Agregar a cache si hay espacio
        pthread_mutex_lock(&g_dfs->cache->lock);
        if (g_dfs->cache->count < g_dfs->cache->capacity) {
            size_t idx = g_dfs->cache->count++;
            g_dfs->cache->entries[idx].block_id = block->block_id;
            g_dfs->cache->entries[idx].data = malloc(DFS_BLOCK_SIZE);
            memcpy(g_dfs->cache->entries[idx].data, block->data, DFS_BLOCK_SIZE);
            g_dfs->cache->entries[idx].size = DFS_BLOCK_SIZE;
            g_dfs->cache->entries[idx].timestamp = time(NULL);
            atomic_store(&g_dfs->cache->entries[idx].hits, 1);
        }
        pthread_mutex_unlock(&g_dfs->cache->lock);
    }
    
    // Actualizar tiempo de acceso
    clock_gettime(CLOCK_REALTIME, &file->accessed);
    atomic_fetch_add(&file->access_count, 1);
    
    pthread_rwlock_unlock(&file->lock);
    
    return bytes_read;
}

// ========================================
// OPERACIONES DE DIRECTORIO
// ========================================

dfs_directory_t* dfs_mkdir(const char* path, uint16_t permissions) {
    pthread_rwlock_wrlock(&g_dfs->global_lock);
    
    dfs_directory_t* dir = (dfs_directory_t*)calloc(1, sizeof(dfs_directory_t));
    dir->inode = atomic_fetch_add(&g_dfs->next_inode, 1);
    
    // Extraer nombre del path
    const char* name = strrchr(path, '/');
    if (name) {
        strncpy(dir->name, name + 1, DFS_MAX_NAME_LEN - 1);
    } else {
        strncpy(dir->name, path, DFS_MAX_NAME_LEN - 1);
    }
    
    dir->parent = g_dfs->root;  // Por simplicidad, todo bajo root
    dir->entry_capacity = 100;
    dir->entries = calloc(dir->entry_capacity, sizeof(dir->entries[0]));
    dir->permissions = permissions;
    dir->uid = getuid();
    dir->gid = getgid();
    
    clock_gettime(CLOCK_REALTIME, &dir->created);
    dir->modified = dir->created;
    
    pthread_rwlock_init(&dir->lock, NULL);
    
    // Agregar al directorio padre
    pthread_rwlock_wrlock(&g_dfs->root->lock);
    if (g_dfs->root->entry_count < g_dfs->root->entry_capacity) {
        size_t idx = g_dfs->root->entry_count++;
        strncpy(g_dfs->root->entries[idx].name, dir->name, DFS_MAX_NAME_LEN - 1);
        g_dfs->root->entries[idx].inode = dir->inode;
        g_dfs->root->entries[idx].type = DFS_TYPE_DIRECTORY;
    }
    pthread_rwlock_unlock(&g_dfs->root->lock);
    
    atomic_fetch_add(&g_dfs->stats.total_directories, 1);
    
    pthread_rwlock_unlock(&g_dfs->global_lock);
    
    printf("[DFS] Directorio creado: %s (inode: %lu)\n", dir->name, dir->inode);
    
    return dir;
}

// Listar contenido de directorio
int dfs_readdir(dfs_directory_t* dir, void (*callback)(const char*, uint64_t, dfs_file_type_t)) {
    if (!dir || !callback) return -EINVAL;
    
    pthread_rwlock_rdlock(&dir->lock);
    
    for (size_t i = 0; i < dir->entry_count; i++) {
        callback(dir->entries[i].name, dir->entries[i].inode, dir->entries[i].type);
    }
    
    pthread_rwlock_unlock(&dir->lock);
    
    return dir->entry_count;
}

// ========================================
// REPLICACIÓN Y DISTRIBUCIÓN
// ========================================

// Replicar bloque a otros nodos
int dfs_replicate_block(dfs_block_t* block, node_id_t* target_nodes, int count) {
    if (!block || !target_nodes) return -EINVAL;
    
    printf("[DFS] Replicando bloque %lu a %d nodos\n", block->block_id, count);
    
    int replicated = 0;
    for (int i = 0; i < count && i < DFS_REPLICATION_FACTOR; i++) {
        // En implementación real, enviar bloque por red
        block->replicas[replicated] = target_nodes[i];
        replicated++;
    }
    
    return replicated;
}

// Distribuir archivo en múltiples nodos (striping)
int dfs_stripe_file(dfs_file_t* file, node_id_t* nodes, int node_count) {
    if (!file || !nodes || node_count == 0) return -EINVAL;
    
    pthread_rwlock_wrlock(&file->lock);
    
    file->stripe_width = (node_count < 8) ? node_count : 8;
    
    // Asignar bloques a nodos en round-robin
    for (size_t i = 0; i < file->blocks_count; i++) {
        int node_idx = i % file->stripe_width;
        
        // Buscar bloque
        for (size_t j = 0; j < g_dfs->block_count; j++) {
            if (g_dfs->block_table[j]->block_id == file->block_list[i]) {
                g_dfs->block_table[j]->primary_node = nodes[node_idx];
                break;
            }
        }
    }
    
    printf("[DFS] Archivo %s distribuido en %d nodos\n", file->name, file->stripe_width);
    
    pthread_rwlock_unlock(&file->lock);
    
    return 0;
}

// ========================================
// ESTADÍSTICAS Y MONITOREO
// ========================================

void dfs_print_stats(void) {
    printf("\n=== ESTADÍSTICAS DEL SISTEMA DE ARCHIVOS ===\n");
    printf("Archivos totales:     %lu\n", atomic_load(&g_dfs->stats.total_files));
    printf("Directorios totales:  %lu\n", atomic_load(&g_dfs->stats.total_directories));
    printf("Bloques totales:      %lu\n", atomic_load(&g_dfs->stats.total_blocks));
    printf("Bytes totales:        %lu MB\n", atomic_load(&g_dfs->stats.total_bytes) / (1024*1024));
    printf("Cache hits:           %lu\n", atomic_load(&g_dfs->stats.cache_hits));
    printf("Cache misses:         %lu\n", atomic_load(&g_dfs->stats.cache_misses));
    
    if (g_dfs->stats.cache_hits + g_dfs->stats.cache_misses > 0) {
        double hit_rate = (double)g_dfs->stats.cache_hits / 
                         (g_dfs->stats.cache_hits + g_dfs->stats.cache_misses) * 100;
        printf("Cache hit rate:       %.2f%%\n", hit_rate);
    }
}

// ========================================
// LIMPIEZA
// ========================================

void dfs_cleanup(void) {
    if (!g_dfs) return;
    
    // Liberar cache
    if (g_dfs->cache) {
        for (size_t i = 0; i < g_dfs->cache->count; i++) {
            free(g_dfs->cache->entries[i].data);
        }
        free(g_dfs->cache->entries);
        pthread_mutex_destroy(&g_dfs->cache->lock);
        free(g_dfs->cache);
    }
    
    // Liberar archivos
    for (size_t i = 0; i < g_dfs->inode_count; i++) {
        if (g_dfs->inode_table[i]) {
            free(g_dfs->inode_table[i]->block_list);
            pthread_rwlock_destroy(&g_dfs->inode_table[i]->lock);
            free(g_dfs->inode_table[i]);
        }
    }
    free(g_dfs->inode_table);
    
    // Liberar bloques
    for (size_t i = 0; i < g_dfs->block_count; i++) {
        free(g_dfs->block_table[i]);
    }
    free(g_dfs->block_table);
    
    // Liberar directorio raíz
    if (g_dfs->root) {
        free(g_dfs->root->entries);
        pthread_rwlock_destroy(&g_dfs->root->lock);
        free(g_dfs->root);
    }
    
    pthread_rwlock_destroy(&g_dfs->global_lock);
    free(g_dfs);
    g_dfs = NULL;
}

#endif // DFS_H
