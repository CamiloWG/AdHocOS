#include "common.h"
#include "scheduler/scheduler.h"
#include "memory/memory_manager.h"
#include "network/network.h"
#include "network/discovery.h"
#include "sync/sync.h"
#include "fault_tolerance/fault_manager.h"
#include "ml/ml_lib.h"

// ========================================
// KERNEL DESCENTRALIZADO PRINCIPAL
// ========================================

typedef struct {
    int node_id;
    NetworkManager* network;
    DiscoveryManager* discovery;
    FaultToleranceManager* fault_tolerance;
    LamportMutex* sync_mutex;
    LogicalClock* logical_clock;
    int running;
} DecentralizedKernel;

DecentralizedKernel* kernel = NULL;
volatile sig_atomic_t shutdown_flag = 0;

// Manejador de señales
void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        log_info("\n🛑 Señal de terminación recibida...");
        shutdown_flag = 1;
        if (kernel) {
            kernel->running = 0;
        }
    }
}

// Inicializar el kernel
DecentralizedKernel* init_kernel(int node_id) {
    kernel = (DecentralizedKernel*)malloc(sizeof(DecentralizedKernel));
    kernel->node_id = node_id;
    kernel->running = 1;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║   SISTEMA OPERATIVO DESCENTRALIZADO v0.2                  ║\n");
    printf("║   Fase 2: Núcleo Funcional Distribuido                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    log_info("Inicializando Nodo %d...", node_id);
    
    // Inicializar subsistemas
    init_scheduler();
    init_memory_manager();
    
    // Crear gestores
    kernel->network = create_network_manager(node_id, NODE_PORT_BASE + node_id);
    kernel->discovery = create_discovery_manager(node_id);
    kernel->fault_tolerance = create_fault_tolerance_manager();
    kernel->sync_mutex = create_lamport_mutex(node_id);
    kernel->logical_clock = create_logical_clock(node_id);
    
    log_info("✅ Kernel inicializado correctamente\n");
    
    return kernel;
}

// Demostración de funcionalidades
void demo_scheduler(DecentralizedKernel* k) {
    log_info("=== DEMO: Scheduler Distribuido ===");
    
    // Simular nodos descubiertos
    simulate_node_discovery(k->discovery, 3);
    
    // Copiar nodos al fault tolerance manager
    k->fault_tolerance->nodes = k->discovery->discovered_nodes;
    k->fault_tolerance->node_count = k->discovery->discovered_count;
    
    // Crear tareas de ejemplo
    for (int i = 0; i < 5; i++) {
        Task task;
        task.task_id = 0; // Se asignará automáticamente
        task.priority = 5 + i;
        task.status = TASK_PENDING;
        task.task_function = NULL;
        task.task_data = NULL;
        task.data_size = 0;
        
        int assigned = schedule_task(&task, k->discovery->discovered_nodes, 
                                     k->discovery->discovered_count);
        if (assigned >= 0) {
            log_info("✓ Tarea %d programada", task.task_id);
        }
        
        usleep(100000); // 100ms
    }
    
    printf("\n");
    print_scheduler_stats();
    printf("\n");
}

void demo_memory(DecentralizedKernel* k) {
    log_info("=== DEMO: Gestión de Memoria Distribuida ===");
    
    // Asignar bloques de memoria
    SharedMemory* mem1 = allocate_shared_memory(1024, k->node_id);
    SharedMemory* mem2 = allocate_shared_memory(2048, k->node_id);
    SharedMemory* mem3 = allocate_shared_memory(512, k->node_id);
    
    // Escribir datos
    char data[] = "Datos de prueba en memoria compartida";
    write_shared_memory(mem1, data, strlen(data) + 1, 0);
    
    // Leer datos
    char buffer[128];
    read_shared_memory(mem1, buffer, strlen(data) + 1, 0);
    log_info("Datos leídos: '%s'", buffer);
    
    // Replicar memoria
    if (k->discovery->discovered_count > 0) {
        replicate_memory(mem1, k->discovery->discovered_nodes[0].node_id);
        replicate_memory(mem2, k->discovery->discovered_nodes[0].node_id);
    }
    
    printf("\n");
    print_memory_stats();
    printf("\n");
}

void demo_synchronization(DecentralizedKernel* k) {
    log_info("=== DEMO: Sincronización Distribuida ===");
    
    // Incrementar reloj lógico
    for (int i = 0; i < 3; i++) {
        int ts = increment_clock(k->logical_clock);
        log_info("Timestamp local: %d", ts);
        usleep(50000);
    }
    
    // Simular recepción de mensaje con timestamp mayor
    int received_ts = 15;
    int updated_ts = update_clock(k->logical_clock, received_ts);
    log_info("Timestamp actualizado tras recibir %d: %d", received_ts, updated_ts);
    
    // Demostrar adquisición de lock distribuido
    log_info("\nProbando lock distribuido...");
    acquire_distributed_lock(k->sync_mutex, 3);
    log_info("Sección crítica ejecutándose...");
    sleep(1);
    release_distributed_lock(k->sync_mutex);
    
    printf("\n");
}

void demo_fault_tolerance(DecentralizedKernel* k) {
    log_info("=== DEMO: Tolerancia a Fallos ===");
    
    // Iniciar monitoreo
    start_fault_tolerance(k->fault_tolerance);
    
    // Crear checkpoint
    create_checkpoint(k->fault_tolerance, "checkpoint_inicial");
    
    // Simular fallo de nodo
    if (k->discovery->discovered_count > 0) {
        log_info("\nSimulando fallo del nodo 1...");
        simulate_node_failure(k->fault_tolerance, 1);
        sleep(2);
        
        // El monitor debería detectarlo
        log_info("Esperando detección de fallo...");
        sleep(NODE_TIMEOUT + 2);
    }
    
    printf("\n");
    print_fault_tolerance_stats(k->fault_tolerance);
    printf("\n");
    
    stop_fault_tolerance(k->fault_tolerance);
}

void demo_ml(DecentralizedKernel* k) {
    log_info("=== DEMO: Machine Learning Básico ===");
    
    // Dataset simple para clasificación binaria
    int n_samples = 100;
    int n_features = 2;
    
    // Crear datos sintéticos
    double** X = (double**)malloc(n_samples * sizeof(double*));
    int* y = (int*)malloc(n_samples * sizeof(int));
    
    for (int i = 0; i < n_samples; i++) {
        X[i] = (double*)malloc(n_features * sizeof(double));
        X[i][0] = (double)(rand() % 100) / 10.0;
        X[i][1] = (double)(rand() % 100) / 10.0;
        y[i] = (X[i][0] + X[i][1] > 10.0) ? 1 : 0;
    }
    
    // Entrenar perceptrón
    log_info("Entrenando Perceptrón...");
    Perceptron* p = create_perceptron(n_features);
    train_perceptron(p, X, y, n_samples);
    
    // Probar predicción
    double test[2] = {8.0, 4.0};
    int pred = predict_perceptron(p, test);
    log_info("Predicción para [8.0, 4.0]: %d", pred);
    
    // Liberar memoria
    for (int i = 0; i < n_samples; i++) {
        free(X[i]);
    }
    free(X);
    free(y);
    destroy_perceptron(p);
    
    printf("\n");
}

void run_interactive_mode(DecentralizedKernel* k) {
    log_info("Modo interactivo activado (presiona Ctrl+C para salir)");
    
    // Iniciar servicios de red
    start_network_manager(k->network);
    start_discovery(k->discovery);
    
    // Simular descubrimiento para pruebas
    simulate_node_discovery(k->discovery, 3);
    k->fault_tolerance->nodes = k->discovery->discovered_nodes;
    k->fault_tolerance->node_count = k->discovery->discovered_count;
    
    start_fault_tolerance(k->fault_tolerance);
    
    int counter = 0;
    while (k->running && !shutdown_flag) {
        // Enviar heartbeat periódicamente
        if (counter % 5 == 0) {
            send_heartbeat(k->network);
        }
        
        // Mostrar estadísticas cada 10 segundos
        if (counter % 10 == 0 && counter > 0) {
            printf("\n");
            log_info("═══ Estadísticas del Sistema (t=%ds) ═══", counter);
            print_scheduler_stats();
            print_memory_stats();
            print_fault_tolerance_stats(k->fault_tolerance);
            printf("\n");
        }
        
        sleep(1);
        counter++;
    }
    
    // Detener servicios
    stop_network_manager(k->network);
    stop_discovery(k->discovery);
    stop_fault_tolerance(k->fault_tolerance);
}

void cleanup_kernel(DecentralizedKernel* k) {
    if (!k) return;
    
    log_info("Limpiando recursos del kernel...");
    
    // Limpiar subsistemas
    cleanup_scheduler();
    cleanup_memory_manager();
    
    // Destruir gestores
    if (k->network) destroy_network_manager(k->network);
    if (k->discovery) destroy_discovery_manager(k->discovery);
    if (k->fault_tolerance) destroy_fault_tolerance_manager(k->fault_tolerance);
    if (k->sync_mutex) destroy_lamport_mutex(k->sync_mutex);
    if (k->logical_clock) destroy_logical_clock(k->logical_clock);
    
    free(k);
    
    log_info("✅ Limpieza completada");
}

// ========================================
// PROGRAMA PRINCIPAL
// ========================================

int main(int argc, char* argv[]) {
    // Configurar manejador de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Obtener ID del nodo
    int node_id = 0;
    if (argc > 1) {
        node_id = atoi(argv[1]);
    }
    
    // Inicializar kernel
    kernel = init_kernel(node_id);
    
    // Modo de operación
    char mode = 'd'; // demo por defecto
    if (argc > 2) {
        mode = argv[2][0];
    }
    
    if (mode == 'i') {
        // Modo interactivo (para cluster)
        run_interactive_mode(kernel);
    } else {
        // Modo demo (ejecutar todas las demos)
        log_info("Ejecutando demostraciones de funcionalidad...\n");
        
        demo_scheduler(kernel);
        sleep(1);
        
        demo_memory(kernel);
        sleep(1);
        
        demo_synchronization(kernel);
        sleep(1);
        
        demo_fault_tolerance(kernel);
        sleep(1);
        
        demo_ml(kernel);
        
        log_info("\n✅ Todas las demostraciones completadas");
    }
    
    // Limpieza
    cleanup_kernel(kernel);
    
    printf("\n");
    log_info("Sistema operativo descentralizado terminado correctamente");
    printf("\n");
    
    return 0;
}