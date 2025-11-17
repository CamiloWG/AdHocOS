// ========================================
// SISTEMA DE LLAMADAS DISTRIBUIDAS (SYSCALLS) - Continuación
// Para Sistema Operativo Descentralizado 64-bit
// ========================================

static inline int dml_train(uint64_t model_type, void* data, size_t size, 
                           void* params, uint32_t nodes) {
    return (int)distributed_syscall(SYS_ML_TRAIN, model_type, 
                                   (uint64_t)data, size, 
                                   (uint64_t)params, nodes, 0);
}

// ========================================
// SYSCALLS ASÍNCRONAS
// ========================================

typedef void (*syscall_callback_t)(syscall_result_t* result);

// Estructura para syscall asíncrona
typedef struct {
    syscall_context_t* context;
    pthread_t handler_thread;
} async_syscall_t;

// Thread para manejar syscall asíncrona
static void* async_syscall_handler(void* arg) {
    syscall_context_t* ctx = (syscall_context_t*)arg;
    
    // Validar y ejecutar syscall
    int idx = ctx->args.syscall_id - 1000;
    if (idx >= 0 && idx < sizeof(syscall_table) / sizeof(syscall_table[0]) &&
        syscall_table[idx].handler) {
        
        syscall_table[idx].handler(&ctx->args, &ctx->result);
    } else {
        ctx->result.error_code = ENOSYS;
        ctx->result.return_value = -1;
    }
    
    // Marcar como completado
    atomic_store(&ctx->completed, 1);
    
    // Señalar completion
    pthread_mutex_lock(&ctx->completion_mutex);
    pthread_cond_signal(&ctx->completion_cond);
    pthread_mutex_unlock(&ctx->completion_mutex);
    
    // Ejecutar callback si existe
    if (ctx->callback) {
        ctx->callback(&ctx->result);
    }
    
    return NULL;
}

// Iniciar syscall asíncrona
static async_syscall_t* distributed_syscall_async(distributed_syscall_t syscall_id,
                                                  syscall_callback_t callback,
                                                  uint64_t arg0, uint64_t arg1,
                                                  uint64_t arg2, uint64_t arg3,
                                                  uint64_t arg4, uint64_t arg5) {
    async_syscall_t* async = calloc(1, sizeof(async_syscall_t));
    async->context = calloc(1, sizeof(syscall_context_t));
    
    // Configurar contexto
    async->context->args.syscall_id = syscall_id;
    async->context->args.args[0] = arg0;
    async->context->args.args[1] = arg1;
    async->context->args.args[2] = arg2;
    async->context->args.args[3] = arg3;
    async->context->args.args[4] = arg4;
    async->context->args.args[5] = arg5;
    async->context->callback = callback;
    
    atomic_store(&async->context->completed, 0);
    pthread_mutex_init(&async->context->completion_mutex, NULL);
    pthread_cond_init(&async->context->completion_cond, NULL);
    
    // Crear thread para manejar syscall
    pthread_create(&async->handler_thread, NULL, async_syscall_handler, async->context);
    
    return async;
}

// Esperar completación de syscall asíncrona
static int distributed_syscall_wait(async_syscall_t* async, syscall_result_t* result) {
    if (!async || !async->context) return -EINVAL;
    
    // Esperar a que se complete
    pthread_mutex_lock(&async->context->completion_mutex);
    while (!atomic_load(&async->context->completed)) {
        pthread_cond_wait(&async->context->completion_cond, 
                         &async->context->completion_mutex);
    }
    pthread_mutex_unlock(&async->context->completion_mutex);
    
    // Copiar resultado
    if (result) {
        memcpy(result, &async->context->result, sizeof(syscall_result_t));
    }
    
    // Limpiar
    pthread_join(async->handler_thread, NULL);
    pthread_mutex_destroy(&async->context->completion_mutex);
    pthread_cond_destroy(&async->context->completion_cond);
    free(async->context);
    free(async);
    
    return 0;
}

// ========================================
// SYSCALLS BATCH (múltiples operaciones)
// ========================================

typedef struct {
    syscall_args_t* requests;
    syscall_result_t* results;
    size_t count;
    _Atomic size_t completed;
} batch_syscall_t;

// Ejecutar batch de syscalls
static int distributed_syscall_batch(batch_syscall_t* batch) {
    if (!batch || !batch->requests || !batch->results) return -EINVAL;
    
    atomic_store(&batch->completed, 0);
    
    // Ejecutar cada syscall
    for (size_t i = 0; i < batch->count; i++) {
        int idx = batch->requests[i].syscall_id - 1000;
        
        if (idx >= 0 && idx < sizeof(syscall_table) / sizeof(syscall_table[0]) &&
            syscall_table[idx].handler) {
            
            syscall_table[idx].handler(&batch->requests[i], &batch->results[i]);
        } else {
            batch->results[i].error_code = ENOSYS;
            batch->results[i].return_value = -1;
        }
        
        atomic_fetch_add(&batch->completed, 1);
    }
    
    return 0;
}

// ========================================
// MONITOREO Y ESTADÍSTICAS DE SYSCALLS
// ========================================

typedef struct {
    _Atomic uint64_t call_count[SYS_MAX_SYSCALL - 1000];
    _Atomic uint64_t total_calls;
    _Atomic uint64_t failed_calls;
    _Atomic uint64_t total_latency_ns;
    _Atomic uint64_t max_latency_ns;
} syscall_stats_t;

static syscall_stats_t g_syscall_stats = {0};

// Wrapper con estadísticas
static int64_t distributed_syscall_monitored(distributed_syscall_t syscall_id, ...) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    syscall_args_t args = {0};
    syscall_result_t result = {0};
    
    args.syscall_id = syscall_id;
    
    // Obtener argumentos
    va_list ap;
    va_start(ap, syscall_id);
    for (int i = 0; i < 6; i++) {
        args.args[i] = va_arg(ap, uint64_t);
    }
    va_end(ap);
    
    // Incrementar contador
    atomic_fetch_add(&g_syscall_stats.total_calls, 1);
    
    if (syscall_id >= 1000 && syscall_id < SYS_MAX_SYSCALL) {
        atomic_fetch_add(&g_syscall_stats.call_count[syscall_id - 1000], 1);
    }
    
    // Ejecutar syscall
    int idx = syscall_id - 1000;
    if (idx >= 0 && idx < sizeof(syscall_table) / sizeof(syscall_table[0]) &&
        syscall_table[idx].handler) {
        
        syscall_table[idx].handler(&args, &result);
    } else {
        result.error_code = ENOSYS;
        result.return_value = -1;
    }
    
    // Calcular latencia
    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t latency_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                         (end.tv_nsec - start.tv_nsec);
    
    atomic_fetch_add(&g_syscall_stats.total_latency_ns, latency_ns);
    
    // Actualizar máxima latencia
    uint64_t max_lat = atomic_load(&g_syscall_stats.max_latency_ns);
    while (latency_ns > max_lat) {
        if (atomic_compare_exchange_weak(&g_syscall_stats.max_latency_ns, 
                                        &max_lat, latency_ns)) {
            break;
        }
    }
    
    if (result.return_value < 0) {
        atomic_fetch_add(&g_syscall_stats.failed_calls, 1);
        errno = result.error_code;
    }
    
    return result.return_value;
}

// Imprimir estadísticas
static void print_syscall_stats(void) {
    uint64_t total = atomic_load(&g_syscall_stats.total_calls);
    uint64_t failed = atomic_load(&g_syscall_stats.failed_calls);
    uint64_t total_lat = atomic_load(&g_syscall_stats.total_latency_ns);
    uint64_t max_lat = atomic_load(&g_syscall_stats.max_latency_ns);
    
    printf("\n=== ESTADÍSTICAS DE SYSCALLS ===\n");
    printf("Total llamadas:     %lu\n", total);
    printf("Llamadas fallidas:  %lu (%.2f%%)\n", failed, 
           total > 0 ? (double)failed / total * 100 : 0);
    printf("Latencia promedio:  %.2f µs\n", 
           total > 0 ? (double)total_lat / total / 1000 : 0);
    printf("Latencia máxima:    %.2f µs\n", (double)max_lat / 1000);
    
    printf("\nTop 5 syscalls más usadas:\n");
    
    // Encontrar top 5
    struct {
        int idx;
        uint64_t count;
    } top5[5] = {0};
    
    for (int i = 0; i < SYS_MAX_SYSCALL - 1000; i++) {
        uint64_t count = atomic_load(&g_syscall_stats.call_count[i]);
        if (count == 0) continue;
        
        // Insertar en top 5
        for (int j = 0; j < 5; j++) {
            if (count > top5[j].count) {
                // Mover hacia abajo
                for (int k = 4; k > j; k--) {
                    top5[k] = top5[k-1];
                }
                top5[j].idx = i;
                top5[j].count = count;
                break;
            }
        }
    }
    
    for (int i = 0; i < 5 && top5[i].count > 0; i++) {
        if (syscall_table[top5[i].idx].name) {
            printf("  %d. %s: %lu llamadas\n", i + 1,
                   syscall_table[top5[i].idx].name, top5[i].count);
        }
    }
}

// ========================================
// SEGURIDAD Y VALIDACIÓN
// ========================================

// Permisos para syscalls
typedef struct {
    uid_t uid;
    gid_t gid;
    uint32_t capabilities;
} syscall_permissions_t;

// Validar permisos para syscall
static int validate_syscall_permissions(distributed_syscall_t syscall_id,
                                       syscall_permissions_t* perms) {
    // Syscalls que requieren privilegios
    switch (syscall_id) {
        case SYS_CHECKPOINT_CREATE:
        case SYS_CHECKPOINT_RESTORE:
        case SYS_SNAPSHOT:
            // Requiere CAP_SYS_ADMIN
            if (!(perms->capabilities & CAP_SYS_ADMIN)) {
                return -EPERM;
            }
            break;
            
        case SYS_MIGRATE_PROCESS:
        case SYS_KILL_DISTRIBUTED:
            // Requiere ser owner del proceso o CAP_KILL
            // Verificación adicional necesaria
            break;
            
        default:
            // Syscall permitida para todos
            break;
    }
    
    return 0;
}

// ========================================
// INICIALIZACIÓN Y CLEANUP
// ========================================

static int g_syscalls_initialized = 0;

// Inicializar sistema de syscalls
int init_distributed_syscalls(void) {
    if (g_syscalls_initialized) return 0;
    
    printf("[SYSCALLS] Inicializando sistema de llamadas distribuidas\n");
    
    // Verificar que todos los handlers estén definidos
    int handlers_count = 0;
    for (int i = 0; i < sizeof(syscall_table) / sizeof(syscall_table[0]); i++) {
        if (syscall_table[i].handler) {
            handlers_count++;
            printf("[SYSCALLS]   - %s registrada\n", syscall_table[i].name);
        }
    }
    
    printf("[SYSCALLS] %d syscalls registradas\n", handlers_count);
    
    // Inicializar estadísticas
    memset(&g_syscall_stats, 0, sizeof(g_syscall_stats));
    
    g_syscalls_initialized = 1;
    
    return 0;
}

// Limpiar sistema de syscalls
void cleanup_distributed_syscalls(void) {
    if (!g_syscalls_initialized) return;
    
    print_syscall_stats();
    
    g_syscalls_initialized = 0;
}

#endif // DISTRIBUTED_SYSCALLS_H
