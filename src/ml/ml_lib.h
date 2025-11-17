#ifndef ML_LIB_H
#define ML_LIB_H

// ========================================
// MODELOS DE MACHINE LEARNING
// ========================================

// Regresión Lineal
typedef struct {
    double* weights;
    double bias;
    int n_features;
    double learning_rate;
    int iterations;
} LinearRegression;

// Regresión Logística
typedef struct {
    double* weights;
    double bias;
    int n_features;
    double learning_rate;
    int iterations;
} LogisticRegression;

// Perceptrón
typedef struct {
    double* weights;
    double bias;
    int n_features;
    double learning_rate;
    int iterations;
} Perceptron;

// ========================================
// FUNCIONES - REGRESIÓN LINEAL
// ========================================

LinearRegression* create_linear_regression(int features);
void train_linear_regression(LinearRegression* lr, double** X, double* y, int n_samples);
double predict_linear_regression(LinearRegression* lr, double* X);
void destroy_linear_regression(LinearRegression* lr);

// ========================================
// FUNCIONES - REGRESIÓN LOGÍSTICA
// ========================================

LogisticRegression* create_logistic_regression(int features);
void train_logistic_regression(LogisticRegression* lr, double** X, int* y, int n_samples);
int predict_logistic_regression(LogisticRegression* lr, double* X);
void destroy_logistic_regression(LogisticRegression* lr);

// ========================================
// FUNCIONES - PERCEPTRÓN
// ========================================

Perceptron* create_perceptron(int features);
void train_perceptron(Perceptron* p, double** X, int* y, int n_samples);
int predict_perceptron(Perceptron* p, double* X);
void destroy_perceptron(Perceptron* p);

// ========================================
// UTILIDADES
// ========================================

double sigmoid(double x);
void normalize_data(double** X, int n_samples, int n_features);
double calculate_accuracy(int* predictions, int* actual, int n_samples);

#endif // ML_LIB_H