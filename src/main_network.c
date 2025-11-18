// main_network.c - Sistema Operativo Descentralizado con Red REAL
// Integración con network_discovery.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Incluir el módulo de red
#include "network_discovery.c"

// ========================================
// ESTRUCTURAS DEL KERNEL
// ========================================

typedef struct {
    uint64_t task_id;
    char description[256];
    int priority;
    uint64_t assigned_node;
    void* (*task_function)(void*);
    void* task_data;
    size_t data_size;
    int status;
    time_t creation_time;
    time_t completion_time;
} DistributedTask;

typedef struct {
    DistributedTask* tasks;
    size_t capacity;
    size_t count;
    pthread_mutex_t lock;
    pthread_cond_t task_available;
} TaskScheduler;

typedef struct {
    uint64_t node_id;
    TaskScheduler* scheduler;
    bool running;
    pthread_t scheduler_thread;
    pthread_t command_thread;
} DistributedKernel;

static DistributedKernel* g_kernel = NULL;

// ========================================
// SCHEDULER CON NODOS REALES
// ========================================

uint64_t find_best_node_for_task(DistributedTask* task) {
    NetworkNode* nodes;
    int count = get_active_nodes(&nodes);
    
    if (count == 0) {
        // No hay otros nodos, ejecutar localmente
        return g_kernel->node_id;
    }
    
    uint64_t best_node = g_kernel->node_id;
    float best_score = 100.0; // Penalización para ejecución local
    
    // Evaluar cada nodo activo
    for (int i = 0; i < count; i++) {
        if (nodes[i].active) {
            // Calcular score basado en carga real
            float score = nodes[i].info.cpu_load * 50 + 
                         nodes[i].info.memory_usage * 50;
            
            if (score < best_score) {
                best_score = score;
                best_node = nodes[i].info.node_id;
            }
        }
    }
    
    return best_node;
}

int schedule_task(DistributedTask* task) {
    if (!g_kernel || !g_kernel->scheduler) return -1;
    
    pthread_mutex_lock(&g_kernel->scheduler->lock);
    
    if (g_kernel->scheduler->count >= g_kernel->scheduler->capacity) {
        pthread_mutex_unlock(&g_kernel->scheduler->lock);
        return -1;
    }
    
    // Encontrar el mejor nodo basado en información REAL
    uint64_t best_node = find_best_node_for_task(task);
    task->assigned_node = best_node;
    task->status = 0; // Pendiente
    task->creation_time = time(NULL);
    
    // Si el nodo asignado no es local, enviar la tarea
    if (best_node != g_kernel->node_id) {
        printf("[SCHEDULER] Enviando tarea %lu al nodo %016lX\n", 
               task->task_id, best_node);
        
        // Serializar y enviar tarea
        char buffer[4096];
        memcpy(buffer, task, sizeof(DistributedTask));
        
        if (send_data_to_node(best_node, buffer, sizeof(DistributedTask)) < 0) {
            printf("[SCHEDULER] Error enviando tarea, ejecutando localmente\n");
            task->assigned_node = g_kernel->node_id;
        }
    }
    
    g_kernel->scheduler->tasks[g_kernel->scheduler->count++] = *task;
    
    pthread_cond_signal(&g_kernel->scheduler->task_available);
    pthread_mutex_unlock(&g_kernel->scheduler->lock);
    
    printf("[SCHEDULER] Tarea %lu asignada al nodo %016lX\n", 
           task->task_id, task->assigned_node);
    
    return 0;
}

// ========================================
// SERVIDOR TCP PARA RECIBIR TAREAS
// ========================================

void* data_server_thread(void* arg) {
    (void)arg;
    
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return NULL;
    }
    
    int reuse = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(DATA_PORT);
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_sock);
        return NULL;
    }
    
    listen(server_sock, 10);
    
    printf("[DATA SERVER] Escuchando en puerto %d\n", DATA_PORT);
    
    while (g_kernel->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;
        
        // Recibir header
        MessageHeader header;
        if (recv(client_sock, &header, sizeof(header), 0) != sizeof(header)) {
            close(client_sock);
            continue;
        }
        
        if (ntohl(header.magic) != 0xDEADBEEF) {
            close(client_sock);
            continue;
        }
        
        uint32_t msg_type = ntohl(header.msg_type);
        uint32_t payload_size = ntohl(header.payload_size);
        
        if (msg_type == MSG_DATA_SYNC && payload_size > 0) {
            char* buffer = malloc(payload_size);
            if (recv(client_sock, buffer, payload_size, 0) == payload_size) {
                // Procesar tarea recibida
                if (payload_size == sizeof(DistributedTask)) {
                    DistributedTask* task = (DistributedTask*)buffer;
                    printf("[DATA SERVER] Tarea recibida: %lu desde nodo %016lX\n",
                           task->task_id, be64toh(header.node_id));
                    
                    // Ejecutar la tarea localmente
                    task->assigned_node = g_kernel->node_id;
                    task->status = 1; // En ejecución
                    
                    // Aquí ejecutarías la tarea real
                    printf("[EXECUTOR] Ejecutando tarea %lu: %s\n", 
                           task->task_id, task->description);
                }
            }
            free(buffer);
        }
        
        close(client_sock);
    }
    
    close(server_sock);
    return NULL;
}

// ========================================
// INTERFAZ DE COMANDOS
// ========================================

void* command_thread(void* arg) {
    (void)arg;
    char command[256];
    
    printf("\nComandos disponibles:\n");
    printf("  status    - Ver estado de la red\n");
    printf("  task <descripción> - Crear nueva tarea\n");
    printf("  tasks     - Ver tareas\n");
    printf("  nodes     - Ver nodos activos\n");
    printf("  exit      - Salir\n\n");
    
    while (g_kernel->running) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        command[strcspn(command, "\n")] = 0; // Eliminar newline
        
        if (strcmp(command, "status") == 0) {
            print_network_status();
        } else if (strcmp(command, "nodes") == 0) {
            NetworkNode* nodes;
            int count = get_active_nodes(&nodes);
            printf("\nNodos activos: %d\n", count);
            for (int i = 0; i < count; i++) {
                if (nodes[i].active) {
                    printf("  - %016lX (%s) en %s\n", 
                           nodes[i].info.node_id,
                           nodes[i].info.hostname,
                           nodes[i].info.ip_address);
                }
            }
        } else if (strncmp(command, "task ", 5) == 0) {
            static uint64_t task_counter = 0;
            DistributedTask task;
            task.task_id = ++task_counter;
            strncpy(task.description, command + 5, sizeof(task.description) - 1);
            task.priority = 5;
            schedule_task(&task);
        } else if (strcmp(command, "tasks") == 0) {
            pthread_mutex_lock(&g_kernel->scheduler->lock);
            printf("\nTareas en el sistema: %zu\n", g_kernel->scheduler->count);
            for (size_t i = 0; i < g_kernel->scheduler->count; i++) {
                DistributedTask* t = &g_kernel->scheduler->tasks[i];
                printf("  [%lu] %s - Nodo: %016lX - Estado: %d\n",
                       t->task_id, t->description, t->assigned_node, t->status);
            }
            pthread_mutex_unlock(&g_kernel->scheduler->lock);
        } else if (strcmp(command, "exit") == 0) {
            g_kernel->running = false;
            break;
        } else if (strlen(command) > 0) {
            printf("Comando desconocido: %s\n", command);
        }
    }
    
    return NULL;
}

// ========================================
// MANEJADOR DE SEÑALES
// ========================================

void handle_signal(int sig) {
    (void)sig;
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
    printf("║   Sistema Operativo Descentralizado - Modo Red REAL       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    // Generar o usar ID del nodo
    uint64_t node_id = 0;
    if (argc > 1) {
        node_id = strtoull(argv[1], NULL, 16);
    }
    
    // Inicializar kernel
    g_kernel = calloc(1, sizeof(DistributedKernel));
    g_kernel->running = true;
    
    // Inicializar red REAL
    if (init_network_discovery(node_id) < 0) {
        fprintf(stderr, "[ERROR] No se pudo inicializar la red\n");
        free(g_kernel);
        return 1;
    }
    
    // Obtener ID del nodo de la red
    if (node_id == 0) {
        g_kernel->node_id = g_network->local_node_id;
    } else {
        g_kernel->node_id = node_id;
    }
    
    // Inicializar scheduler
    g_kernel->scheduler = calloc(1, sizeof(TaskScheduler));
    g_kernel->scheduler->capacity = 1000;
    g_kernel->scheduler->tasks = calloc(g_kernel->scheduler->capacity, 
                                       sizeof(DistributedTask));
    pthread_mutex_init(&g_kernel->scheduler->lock, NULL);
    pthread_cond_init(&g_kernel->scheduler->task_available, NULL);
    
    // Configurar señales
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Iniciar servidor de datos
    pthread_t data_thread;
    pthread_create(&data_thread, NULL, data_server_thread, NULL);
    
    // Esperar un poco para que la red se estabilice
    printf("\n[SISTEMA] Descubriendo nodos en la red...\n");
    sleep(3);
    
    // Mostrar estado inicial
    print_network_status();
    
    // Iniciar interfaz de comandos
    pthread_create(&g_kernel->command_thread, NULL, command_thread, NULL);
    
    // Loop principal
    while (g_kernel->running) {
        sleep(1);
    }
    
    // Limpieza
    printf("\n[SISTEMA] Limpiando recursos...\n");
    
    shutdown_network_discovery();
    
    pthread_mutex_destroy(&g_kernel->scheduler->lock);
    pthread_cond_destroy(&g_kernel->scheduler->task_available);
    
    free(g_kernel->scheduler->tasks);
    free(g_kernel->scheduler);
    free(g_kernel);
    
    printf("[SISTEMA] Apagado completo\n");
    
    return 0;
}