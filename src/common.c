#include "common.h"
#include <stdarg.h>
#include <sys/time.h>

// ========================================
// IMPLEMENTACIÓN DE FUNCIONES DE LOGGING
// ========================================

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[INFO] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void log_debug(const char* format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    printf("[DEBUG] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
#else
    (void)format; // Evitar warning de variable no usada
#endif
}

// ========================================
// FUNCIONES AUXILIARES
// ========================================

unsigned long get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void print_node_info(Node* node) {
    if (!node) return;
    
    printf("Nodo ID: %d\n", node->node_id);
    printf("  IP: %s:%d\n", node->ip_address, node->port);
    printf("  Estado: %d\n", node->status);
    printf("  CPU Load: %.2f%%\n", node->cpu_load * 100);
    printf("  Memory Usage: %.2f%%\n", node->memory_usage * 100);
    printf("  Reputación: %.2f\n", node->reputation);
}

void print_task_info(Task* task) {
    if (!task) return;
    
    printf("Tarea ID: %d\n", task->task_id);
    printf("  Prioridad: %d\n", task->priority);
    printf("  Nodo asignado: %d\n", task->assigned_node);
    printf("  Estado: %d\n", task->status);
}