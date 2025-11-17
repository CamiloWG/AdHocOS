#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

// ========================================
// CONSTANTES GLOBALES
// ========================================

#define MAX_NODES 100
#define MAX_TASKS 1000
#define MAX_MEMORY_BLOCKS 1000
#define BUFFER_SIZE 1024
#define NODE_PORT_BASE 8080
#define DISCOVERY_PORT 9999
#define HEARTBEAT_INTERVAL 5
#define NODE_TIMEOUT 15

// ========================================
// ENUMERACIONES
// ========================================

typedef enum {
    NODE_IDLE,
    NODE_BUSY,
    NODE_OFFLINE,
    NODE_FAILED
} NodeStatus;

typedef enum {
    TASK_PENDING = 0,
    TASK_RUNNING = 1,
    TASK_COMPLETED = 2,
    TASK_FAILED = 3
} TaskStatus;

typedef enum {
    MSG_HEARTBEAT = 0,
    MSG_TASK = 1,
    MSG_DATA = 2,
    MSG_SYNC = 3,
    MSG_DISCOVERY = 4,
    MSG_LOCK_REQUEST = 5,
    MSG_LOCK_RELEASE = 6
} MessageType;

// ========================================
// ESTRUCTURAS PRINCIPALES
// ========================================

// Nodo en la red
typedef struct {
    int node_id;
    char ip_address[16];
    int port;
    NodeStatus status;
    float cpu_load;
    float memory_usage;
    float reputation;
    time_t last_heartbeat;
    int task_count;
} Node;

// Tarea distribuida
typedef struct {
    int task_id;
    int priority;
    int assigned_node;
    void* (*task_function)(void*);
    void* task_data;
    int data_size;
    TaskStatus status;
    time_t creation_time;
    time_t completion_time;
} Task;

// Memoria compartida
typedef struct {
    int memory_id;
    void* data;
    size_t size;
    int owner_node;
    int reference_count;
    pthread_mutex_t lock;
    int replicated_nodes[MAX_NODES];
    int replication_count;
} SharedMemory;

// Mensaje entre nodos
typedef struct {
    MessageType type;
    int source_node;
    int dest_node;
    char data[BUFFER_SIZE];
    int data_size;
    time_t timestamp;
} Message;

// Lock distribuido
typedef struct {
    int lock_id;
    int owner_node;
    int timestamp;
    int waiting_nodes[MAX_NODES];
    int waiting_count;
    pthread_mutex_t internal_lock;
} DistributedLock;

// ========================================
// FUNCIONES AUXILIARES
// ========================================

// Logging
void log_info(const char* format, ...);
void log_error(const char* format, ...);
void log_debug(const char* format, ...);

// Utilidades
unsigned long get_timestamp_ms();
void print_node_info(Node* node);
void print_task_info(Task* task);

#endif // COMMON_H