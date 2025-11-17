// ========================================
// PROGRAMA PRINCIPAL - SISTEMA OPERATIVO DESCENTRALIZADO
// Fase 2: Demostración Completa (64-bit)
// ========================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include "kernel_64bit.c"
#include "dfs.h"
#include "distributed_syscalls.h"

// ========================================
// VARIABLES GLOBALES
// ========================================

static volatile int g_running = 1;
static DistributedKernel64* g_kernel = NULL;
static dfs_t* g_filesystem = NULL;

// ========================================
// MANEJADOR DE SEÑALES
// ========================================

void signal_handler(int sig) {
    printf("\n[MAIN] Señal %d recibida, terminando...\n", sig);
    g_running = 0;
    if (g_kernel) {
        atomic_store(&g_kernel->running, 0);
    }
}

// ========================================
// DEMOS DE FUNCIONALIDAD
// ========================================

// Demo 1: Crear y ejecutar procesos distribuidos
void demo_distributed_processes(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  DEMO 1: PROCESOS DISTRIBUIDOS                                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Crear varios procesos
    for (int i = 0; i < 3; i++) {
        Task64* task = create_task_64(example_ml_task, NULL, 0);
        if (task) {
            printf("[DEMO] Proceso creado - PID: %lu\n", task->task_id);
            
            // Asignar a nodo usando scheduler inteligente
            node_id_t node = intelligent_task_assignment(task, 
                                                         g_kernel->node_table, 3);
            if (node != (node_id_t)-1) {
                task->assigned_node = node;
                atomic_store(&task->status, 1);  // Running
                printf("[DEMO] Proceso %lu asignado al nodo %lu\n", 
                       task->task_id, node);
            }
        }
    }
    
    // Fork distribuido
    printf("\n[DEMO] Probando fork distribuido...\n");
    pid_t child = dfork(1);  // Fork en nodo 1
    if (child > 0) {
        printf("[DEMO] Fork exitoso - PID hijo: %d\n", child);
    }
    
    // Migración de proceso
    printf("\n[DEMO] Migrando proceso 1 al nodo 2...\n");
    if (dmigrate(1, 2) == 0) {
        printf("[DEMO] Migración completada\n");
    }
}

// Demo 2: Sistema de archivos distribuido
void demo_distributed_filesystem(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  DEMO 2: SISTEMA DE ARCHIVOS DISTRIBUIDO                        ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Crear directorios
    dfs_directory_t* dir1 = dfs_mkdir("/home", 0755);
    dfs_directory_t* dir2 = dfs_mkdir("/data", 0755);
    dfs_directory_t* dir3 = dfs_mkdir("/ml_models", 0755);
    
    // Crear archivos
    printf("[DEMO] Creando archivos...\n");
    dfs_file_t* file1 = dfs_create_file("/home/test.txt", 0644);
    dfs_file_t* file2 = dfs_create_file("/data/dataset.bin", 0644);
    dfs_file_t* file3 = dfs_create_file("/ml_models/model.pkl", 0644);
    
    // Escribir datos
    if (file1) {
        const char* text = "Sistema Operativo Descentralizado v2.0\n"
                          "Arquitectura de 64 bits\n"
                          "Soporte para computación distribuida\n";
        ssize_t written = dfs_write(file1, text, strlen(text), 0);
        printf("[DEMO] Escritos %zd bytes en test.txt\n", written);
    }
    
    // Escribir archivo grande
    if (file2) {
        size_t data_size = 1024 * 1024 * 5;  // 5MB
        uint8_t* data = malloc(data_size);
        
        // Llenar con patrón
        for (size_t i = 0; i < data_size; i++) {
            data[i] = i % 256;
        }
        
        printf("[DEMO] Escribiendo archivo de 5MB...\n");
        ssize_t written = dfs_write(file2, data, data_size, 0);
        printf("[DEMO] Escritos %zd MB en dataset.bin\n", written / (1024*1024));
        
        // Verificar lectura
        uint8_t* read_buffer = malloc(data_size);
        ssize_t read_bytes = dfs_read(file2, read_buffer, data_size, 0);
        
        // Verificar integridad
        int errors = 0;
        for (size_t i = 0; i < data_size && i < read_bytes; i++) {
            if (read_buffer[i] != data[i]) errors++;
        }
        
        if (errors == 0) {
            printf("[DEMO] ✅ Verificación de integridad exitosa\n");
        } else {
            printf("[DEMO] ⚠️  %d errores en verificación\n", errors);
        }
        
        free(data);
        free(read_buffer);
    }
    
    // Replicar archivo
    if (file2) {
        node_id_t replicas[] = {1, 2};
        dfs_stripe_file(file2, replicas, 2);
        printf("[DEMO] Archivo replicado en 2 nodos\n");
    }
    
    // Listar directorio raíz
    printf("\n[DEMO] Contenido del directorio raíz:\n");
    dfs_readdir(g_filesystem->root, 
                [](const char* name, uint64_t inode, dfs_file_type_t type) {
                    const char* type_str = (type == DFS_TYPE_DIRECTORY) ? "DIR " : "FILE";
                    printf("  [%s] %s (inode: %lu)\n", type_str, name, inode);
                });
}

// Demo 3: Memoria compartida distribuida
void demo_distributed_memory(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  DEMO 3: MEMORIA COMPARTIDA DISTRIBUIDA                         ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Crear segmentos de memoria compartida
    printf("[DEMO] Creando memoria compartida...\n");
    
    SharedMemory64* mem1 = create_shared_memory_mmap(1024 * 1024, 0);      // 1MB
    SharedMemory64* mem2 = create_shared_memory_mmap(10 * 1024 * 1024, 0); // 10MB
    SharedMemory64* mem3 = create_shared_memory_mmap(100 * 1024 * 1024, 0);// 100MB
    
    // Probar locks de lectura/escritura
    printf("\n[DEMO] Probando locks distribuidos...\n");
    
    // Escritura concurrente
    if (mem1) {
        acquire_write_lock_64(mem1);
        printf("[DEMO] Lock de escritura adquirido\n");
        
        // Escribir datos
        strcpy((char*)mem1->mmap_addr, "Datos críticos protegidos por lock");
        
        release_write_lock_64(mem1);
        printf("[DEMO] Lock de escritura liberado\n");
    }
    
    // Múltiples lectores
    if (mem1) {
        acquire_read_lock_64(mem1);
        printf("[DEMO] Lock de lectura 1 adquirido\n");
        
        acquire_read_lock_64(mem1);
        printf("[DEMO] Lock de lectura 2 adquirido (múltiples lectores OK)\n");
        
        // Leer datos
        printf("[DEMO] Datos leídos: %s\n", (char*)mem1->mmap_addr);
        
        release_read_lock_64(mem1);
        release_read_lock_64(mem1);
        printf("[DEMO] Locks de lectura liberados\n");
    }
    
    // Test de rendimiento con operaciones SIMD
    printf("\n[DEMO] Test de rendimiento con SIMD...\n");
    
    size_t vector_size = 1000000;  // 1M elementos
    double* vec_a = aligned_alloc(32, vector_size * sizeof(double));
    double* vec_b = aligned_alloc(32, vector_size * sizeof(double));
    double* matrix = aligned_alloc(32, 1000 * 1000 * sizeof(double));
    
    // Inicializar vectores
    for (size_t i = 0; i < vector_size; i++) {
        vec_a[i] = (double)rand() / RAND_MAX;
        vec_b[i] = (double)rand() / RAND_MAX;
    }
    
    // Comparar producto punto normal vs SIMD
    uint64_t start = rdtsc();
    double result_normal = 0;
    for (size_t i = 0; i < vector_size; i++) {
        result_normal += vec_a[i] * vec_b[i];
    }
    uint64_t cycles_normal = rdtsc() - start;
    
    start = rdtsc();
    double result_simd = dot_product_avx2(vec_a, vec_b, vector_size);
    uint64_t cycles_simd = rdtsc() - start;
    
    printf("[DEMO] Producto punto (1M elementos):\n");
    printf("  Normal: %.6f (ciclos: %lu)\n", result_normal, cycles_normal);
    printf("  SIMD:   %.6f (ciclos: %lu)\n", result_simd, cycles_simd);
    printf("  Aceleración: %.2fx\n", (double)cycles_normal / cycles_simd);
    
    free(vec_a);
    free(vec_b);
    free(matrix);
}

// Demo 4: Machine Learning distribuido
void demo_distributed_ml(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  DEMO 4: MACHINE LEARNING DISTRIBUIDO                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Crear dataset sintético
    size_t n_samples = 10000;
    size_t n_features = 100;
    
    printf("[DEMO] Creando dataset: %zu muestras, %zu features\n", n_samples, n_features);
    
    double** X = malloc(n_samples * sizeof(double*));
    double* y = malloc(n_samples * sizeof(double));
    
    for (size_t i = 0; i < n_samples; i++) {
        X[i] = aligned_alloc(32, n_features * sizeof(double));
        
        // Generar datos aleatorios
        double sum = 0;
        for (size_t j = 0; j < n_features; j++) {
            X[i][j] = (double)rand() / RAND_MAX;
            sum += X[i][j] * (j % 10);  // Pesos artificiales
        }
        y[i] = sum + ((double)rand() / RAND_MAX - 0.5);  // Con ruido
    }
    
    // Entrenar modelo con SIMD
    printf("[DEMO] Entrenando modelo de regresión lineal...\n");
    
    double* weights = aligned_alloc(32, n_features * sizeof(double));
    memset(weights, 0, n_features * sizeof(double));
    
    double learning_rate = 0.01;
    int epochs = 10;
    
    uint64_t start_training = rdtsc();
    
    for (int epoch = 0; epoch < epochs; epoch++) {
        double total_loss = 0;
        
        for (size_t i = 0; i < n_samples; i++) {
            // Predicción usando SIMD
            double pred = dot_product_avx2(weights, X[i], n_features);
            double error = pred - y[i];
            total_loss += error * error;
            
            // Actualizar pesos (gradiente descendente)
            for (size_t j = 0; j < n_features; j++) {
                weights[j] -= learning_rate * error * X[i][j] / n_samples;
            }
        }
        
        if (epoch % 2 == 0) {
            printf("  Época %d: MSE = %.4f\n", epoch, total_loss / n_samples);
        }
    }
    
    uint64_t training_cycles = rdtsc() - start_training;
    printf("[DEMO] Entrenamiento completado en %lu M ciclos\n", training_cycles / 1000000);
    
    // Simular entrenamiento distribuido
    printf("\n[DEMO] Simulando entrenamiento distribuido en 3 nodos...\n");
    
    size_t samples_per_node = n_samples / 3;
    printf("[DEMO] Cada nodo procesa %zu muestras\n", samples_per_node);
    
    // Syscall para entrenamiento ML distribuido
    int result = dml_train(1, X, n_samples * n_features * sizeof(double), weights, 3);
    if (result == 0) {
        printf("[DEMO] Entrenamiento distribuido iniciado exitosamente\n");
    }
    
    // Liberar memoria
    for (size_t i = 0; i < n_samples; i++) {
        free(X[i]);
    }
    free(X);
    free(y);
    free(weights);
}

// Demo 5: Tolerancia a fallos
void demo_fault_tolerance(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  DEMO 5: TOLERANCIA A FALLOS Y RECUPERACIÓN                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Crear checkpoint
    printf("[DEMO] Creando checkpoint del sistema...\n");
    int cp_result = dcheckpoint("checkpoint_demo", 0);
    if (cp_result == 0) {
        printf("[DEMO] ✅ Checkpoint creado exitosamente\n");
    }
    
    // Simular fallo de nodo
    printf("\n[DEMO] Simulando fallo del nodo 1...\n");
    if (g_kernel->node_table[1].node_id == 1) {
        atomic_store(&g_kernel->node_table[1].status, 3);  // NODE_FAILED
        printf("[DEMO] Nodo 1 marcado como fallido\n");
    }
    
    // Sistema debe redistribuir tareas automáticamente
    printf("[DEMO] Redistribuyendo tareas del nodo fallido...\n");
    
    int tasks_migrated = 0;
    for (int i = 0; i < 100; i++) {
        if (g_kernel->task_table[i].assigned_node == 1 &&
            atomic_load(&g_kernel->task_table[i].status) == 1) {
            
            // Reasignar a otro nodo
            node_id_t new_node = intelligent_task_assignment(&g_kernel->task_table[i],
                                                            g_kernel->node_table, 3);
            if (new_node != (node_id_t)-1 && new_node != 1) {
                g_kernel->task_table[i].assigned_node = new_node;
                tasks_migrated++;
                printf("[DEMO] Tarea %lu reasignada al nodo %lu\n",
                       g_kernel->task_table[i].task_id, new_node);
            }
        }
    }
    
    printf("[DEMO] %d tareas migradas exitosamente\n", tasks_migrated);
    
    // Simular recuperación de nodo
    printf("\n[DEMO] Recuperando nodo 1...\n");
    atomic_store(&g_kernel->node_table[1].status, 1);  // NODE_IDLE
    g_kernel->node_table[1].reputation_score *= 0.8;  // Penalizar reputación
    printf("[DEMO] Nodo 1 recuperado (reputación reducida a %.2f)\n",
           g_kernel->node_table[1].reputation_score);
}

// ========================================
// PROGRAMA PRINCIPAL
// ========================================

int main(int argc, char* argv[]) {
    // Instalar manejador de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Obtener ID del nodo
    node_id_t node_id = 0;
    if (argc > 1) {
        node_id = strtoull(argv[1], NULL, 10);
    }
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                  ║\n");
    printf("║     SISTEMA OPERATIVO DESCENTRALIZADO v2.0 (64-bit)            ║\n");
    printf("║     Fase 2: Núcleo Funcional Distribuido Completo              ║\n");
    printf("║                                                                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Inicializar kernel
    printf("[MAIN] Inicializando kernel distribuido...\n");
    if (init_distributed_kernel_64(node_id) < 0) {
        fprintf(stderr, "[ERROR] Fallo en inicialización del kernel\n");
        return EXIT_FAILURE;
    }
    g_kernel = kernel64;
    
    // Inicializar sistema de archivos
    printf("[MAIN] Inicializando sistema de archivos distribuido...\n");
    g_filesystem = dfs_init(256);  // 256MB de cache
    
    // Inicializar syscalls
    printf("[MAIN] Inicializando sistema de llamadas distribuidas...\n");
    init_distributed_syscalls();
    
    // Crear nodos simulados
    printf("[MAIN] Configurando cluster de 3 nodos...\n");
    for (int i = 0; i < 3; i++) {
        Node64* node = &g_kernel->node_table[i];
        node->node_id = i;
        snprintf(node->ip_address, sizeof(node->ip_address), "192.168.1.%d", 100 + i);
        node->port = 8080 + i;
        atomic_store(&node->status, 1);  // NODE_IDLE
        node->cpu_cores = 4 + i * 2;
        node->cpu_frequency_mhz = 2400 + i * 200;
        node->total_memory_gb = 8 * (i + 1);
        node->available_memory_gb = node->total_memory_gb * 0.7;
        node->cpu_load = 20.0 + (i * 10);
        node->memory_usage = 30.0 + (i * 5);
        node->network_bandwidth_mbps = 1000 + i * 500;
        node->reputation_score = 0.9 - (i * 0.05);
        clock_gettime(CLOCK_MONOTONIC, &node->last_heartbeat);
        
        printf("  Nodo %lu: %s:%d (%lu cores, %luGB RAM, %.0f Mbps)\n",
               node->node_id, node->ip_address, node->port,
               node->cpu_cores, node->total_memory_gb, node->network_bandwidth_mbps);
    }
    
    // Ejecutar demos
    if (argc > 2 && strcmp(argv[2], "demo") == 0) {
        demo_distributed_processes();
        sleep(1);
        
        demo_distributed_filesystem();
        sleep(1);
        
        demo_distributed_memory();
        sleep(1);
        
        demo_distributed_ml();
        sleep(1);
        
        demo_fault_tolerance();
        
        // Estadísticas finales
        printf("\n");
        printf("╔══════════════════════════════════════════════════════════════════╗\n");
        printf("║  ESTADÍSTICAS FINALES DEL SISTEMA                               ║\n");
        printf("╚══════════════════════════════════════════════════════════════════╝\n");
        printf("\n");
        
        // Estadísticas del kernel
        printf("=== KERNEL ===\n");
        printf("Tareas creadas:      %lu\n", atomic_load(&g_kernel->next_task_id) - 1);
        printf("Memoria asignada:    %lu bloques\n", atomic_load(&g_kernel->next_memory_id) - 1);
        printf("Mensajes de red:     %lu\n", atomic_load(&g_kernel->stats.total_network_messages));
        
        // Estadísticas del sistema de archivos
        dfs_print_stats();
        
        // Estadísticas de syscalls
        print_syscall_stats();
        
    } else {
        // Modo interactivo
        printf("\n[MAIN] Sistema en modo interactivo\n");
        printf("[MAIN] Comandos: 'help', 'status', 'demo N', 'exit'\n\n");
        
        char command[256];
        while (g_running) {
            printf("> ");
            if (fgets(command, sizeof(command), stdin) == NULL) break;
            
            // Eliminar newline
            command[strcspn(command, "\n")] = 0;
            
            if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
                break;
            } else if (strcmp(command, "help") == 0) {
                printf("Comandos disponibles:\n");
                printf("  demo 1 - Procesos distribuidos\n");
                printf("  demo 2 - Sistema de archivos\n");
                printf("  demo 3 - Memoria compartida\n");
                printf("  demo 4 - Machine Learning\n");
                printf("  demo 5 - Tolerancia a fallos\n");
                printf("  status - Estado del sistema\n");
                printf("  exit   - Salir\n");
            } else if (strcmp(command, "status") == 0) {
                printf("\n=== ESTADO DEL SISTEMA ===\n");
                printf("Nodo ID: %lu\n", g_kernel->node_id);
                printf("Kernel version: %04X\n", g_kernel->kernel_version);
                printf("CPUs: %lu cores\n", g_kernel->system_info.total_cores);
                printf("RAM: %lu GB\n", g_kernel->system_info.total_memory / (1024*1024*1024));
                
                int active_nodes = 0;
                for (int i = 0; i < 10; i++) {
                    if (atomic_load(&g_kernel->node_table[i].status) == 1) {
                        active_nodes++;
                    }
                }
                printf("Nodos activos: %d\n", active_nodes);
                printf("\n");
            } else if (strncmp(command, "demo ", 5) == 0) {
                int demo_num = atoi(command + 5);
                switch (demo_num) {
                    case 1: demo_distributed_processes(); break;
                    case 2: demo_distributed_filesystem(); break;
                    case 3: demo_distributed_memory(); break;
                    case 4: demo_distributed_ml(); break;
                    case 5: demo_fault_tolerance(); break;
                    default: printf("Demo %d no existe\n", demo_num);
                }
            }
        }
    }
    
    // Limpieza
    printf("\n[MAIN] Limpiando recursos...\n");
    
    cleanup_distributed_syscalls();
    dfs_cleanup();
    
    if (g_kernel) {
        free(g_kernel->task_table);
        free(g_kernel->node_table);
        free(g_kernel->memory_table);
        free(g_kernel);
    }
    
    printf("[MAIN] ✅ Sistema terminado correctamente\n\n");
    
    return EXIT_SUCCESS;
}
