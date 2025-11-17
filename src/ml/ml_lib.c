#include "../common.h"
#include "ml_lib.h"
#include <math.h>

// ========================================
// LIBRERÍA DE MACHINE LEARNING BÁSICA
// (Para Fase 3, pero con estructuras básicas en Fase 2)
// ========================================

// Regresión Lineal
LinearRegression* create_linear_regression(int features) {
    LinearRegression* lr = (LinearRegression*)malloc(sizeof(LinearRegression));
    lr->n_features = features;
    lr->weights = (double*)calloc(features, sizeof(double));
    lr->bias = 0.0;
    lr->learning_rate = 0.01;
    lr->iterations = 1000;
    
    log_info("Modelo de Regresión Lineal creado (%d features)", features);
    return lr;
}

void train_linear_regression(LinearRegression* lr, double** X, double* y, int n_samples) {
    log_info("Entrenando Regresión Lineal...");
    
    for (int iter = 0; iter < lr->iterations; iter++) {
        double total_error = 0.0;
        
        for (int i = 0; i < n_samples; i++) {
            // Predicción
            double pred = lr->bias;
            for (int j = 0; j < lr->n_features; j++) {
                pred += lr->weights[j] * X[i][j];
            }
            
            // Error
            double error = pred - y[i];
            total_error += error * error;
            
            // Actualizar pesos (Gradiente descendente)
            for (int j = 0; j < lr->n_features; j++) {
                lr->weights[j] -= lr->learning_rate * error * X[i][j] / n_samples;
            }
            lr->bias -= lr->learning_rate * error / n_samples;
        }
        
        if (iter % 100 == 0) {
            log_debug("Iteración %d, MSE: %.4f", iter, total_error / n_samples);
        }
    }
    
    log_info("✅ Entrenamiento completado");
}

double predict_linear_regression(LinearRegression* lr, double* X) {
    double pred = lr->bias;
    for (int i = 0; i < lr->n_features; i++) {
        pred += lr->weights[i] * X[i];
    }
    return pred;
}

void destroy_linear_regression(LinearRegression* lr) {
    if (lr) {
        free(lr->weights);
        free(lr);
    }
}

// Regresión Logística
LogisticRegression* create_logistic_regression(int features) {
    LogisticRegression* lr = (LogisticRegression*)malloc(sizeof(LogisticRegression));
    lr->n_features = features;
    lr->weights = (double*)calloc(features, sizeof(double));
    lr->bias = 0.0;
    lr->learning_rate = 0.01;
    lr->iterations = 1000;
    
    log_info("Modelo de Regresión Logística creado (%d features)", features);
    return lr;
}

double sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

void train_logistic_regression(LogisticRegression* lr, double** X, int* y, int n_samples) {
    log_info("Entrenando Regresión Logística...");
    
    for (int iter = 0; iter < lr->iterations; iter++) {
        double total_loss = 0.0;
        
        for (int i = 0; i < n_samples; i++) {
            // Predicción
            double z = lr->bias;
            for (int j = 0; j < lr->n_features; j++) {
                z += lr->weights[j] * X[i][j];
            }
            double pred = sigmoid(z);
            
            // Loss
            total_loss += -(y[i] * log(pred) + (1 - y[i]) * log(1 - pred));
            
            // Actualizar pesos
            double error = pred - y[i];
            for (int j = 0; j < lr->n_features; j++) {
                lr->weights[j] -= lr->learning_rate * error * X[i][j] / n_samples;
            }
            lr->bias -= lr->learning_rate * error / n_samples;
        }
        
        if (iter % 100 == 0) {
            log_debug("Iteración %d, Loss: %.4f", iter, total_loss / n_samples);
        }
    }
    
    log_info("✅ Entrenamiento completado");
}

int predict_logistic_regression(LogisticRegression* lr, double* X) {
    double z = lr->bias;
    for (int i = 0; i < lr->n_features; i++) {
        z += lr->weights[i] * X[i];
    }
    return sigmoid(z) >= 0.5 ? 1 : 0;
}

void destroy_logistic_regression(LogisticRegression* lr) {
    if (lr) {
        free(lr->weights);
        free(lr);
    }
}

// Perceptrón
Perceptron* create_perceptron(int features) {
    Perceptron* p = (Perceptron*)malloc(sizeof(Perceptron));
    p->n_features = features;
    p->weights = (double*)calloc(features, sizeof(double));
    p->bias = 0.0;
    p->learning_rate = 0.1;
    p->iterations = 100;
    
    log_info("Perceptrón creado (%d features)", features);
    return p;
}

void train_perceptron(Perceptron* p, double** X, int* y, int n_samples) {
    log_info("Entrenando Perceptrón...");
    
    for (int iter = 0; iter < p->iterations; iter++) {
        int errors = 0;
        
        for (int i = 0; i < n_samples; i++) {
            // Predicción
            double activation = p->bias;
            for (int j = 0; j < p->n_features; j++) {
                activation += p->weights[j] * X[i][j];
            }
            int pred = activation >= 0.0 ? 1 : 0;
            
            // Actualizar si hay error
            if (pred != y[i]) {
                errors++;
                int update = y[i] - pred;
                for (int j = 0; j < p->n_features; j++) {
                    p->weights[j] += p->learning_rate * update * X[i][j];
                }
                p->bias += p->learning_rate * update;
            }
        }
        
        if (errors == 0) {
            log_info("✅ Convergencia alcanzada en iteración %d", iter);
            break;
        }
    }
    
    log_info("✅ Entrenamiento completado");
}

int predict_perceptron(Perceptron* p, double* X) {
    double activation = p->bias;
    for (int i = 0; i < p->n_features; i++) {
        activation += p->weights[i] * X[i];
    }
    return activation >= 0.0 ? 1 : 0;
}

void destroy_perceptron(Perceptron* p) {
    if (p) {
        free(p->weights);
        free(p);
    }
}

// Funciones auxiliares
void normalize_data(double** X, int n_samples, int n_features) {
    for (int j = 0; j < n_features; j++) {
        double mean = 0.0, std = 0.0;
        
        // Calcular media
        for (int i = 0; i < n_samples; i++) {
            mean += X[i][j];
        }
        mean /= n_samples;
        
        // Calcular desviación estándar
        for (int i = 0; i < n_samples; i++) {
            std += (X[i][j] - mean) * (X[i][j] - mean);
        }
        std = sqrt(std / n_samples);
        
        // Normalizar
        if (std > 0.0) {
            for (int i = 0; i < n_samples; i++) {
                X[i][j] = (X[i][j] - mean) / std;
            }
        }
    }
    
    log_debug("Datos normalizados");
}

double calculate_accuracy(int* predictions, int* actual, int n_samples) {
    int correct = 0;
    for (int i = 0; i < n_samples; i++) {
        if (predictions[i] == actual[i]) {
            correct++;
        }
    }
    return (double)correct / n_samples;
}