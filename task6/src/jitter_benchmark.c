#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <string.h>

#define NUM_ITERATIONS 1000
#define MATRIX_SIZE 10

// Функция для вычисления разницы времени в наносекундах
long long timespec_diff_ns(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
}

// CPU-bound функция: умножение матриц 10x10
void matrix_multiply() {
    double A[MATRIX_SIZE][MATRIX_SIZE];
    double B[MATRIX_SIZE][MATRIX_SIZE];
    double C[MATRIX_SIZE][MATRIX_SIZE];
    
    // Инициализация матриц
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            A[i][j] = (double)rand() / RAND_MAX;
            B[i][j] = (double)rand() / RAND_MAX;
            C[i][j] = 0.0;
        }
    }
    
    // Умножение матриц
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            for (int k = 0; k < MATRIX_SIZE; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

// Функция для вычисления стандартного отклонения
double calculate_stddev(long long latencies[], double mean, int count) {
    double sum_sq_diff = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = (double)latencies[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sqrt(sum_sq_diff / count);
}

int main(int argc, char *argv[]) {
    int target_cpu = -1;
    
    // Парсинг аргументов командной строки
    if (argc > 1) {
        target_cpu = atoi(argv[1]);
        printf("Target CPU specified: %d\n", target_cpu);
    }
    
    printf("=== JITTER BENCHMARK ===\n");
    printf("PID: %d\n", getpid());
    
    /* --- ЗАДАНИЕ 1: УСТАНОВКА REAL-TIME ПРИОРИТЕТА --- */
    struct sched_param sp;
    sp.sched_priority = 50; // Приоритет от 1 до 99 для SCHED_FIFO
    
    if (sched_setscheduler(0, SCHED_FIFO, &sp) == -1) {
        perror("sched_setscheduler failed. Try with sudo.");
        return 1;
    }
    printf("Scheduler policy set to SCHED_FIFO with priority %d\n", sp.sched_priority);
    
    /* --- ЗАДАНИЕ 2: УСТАНОВКА CPU AFFINITY --- */
    if (target_cpu != -1) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(target_cpu, &mask);
        
        if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
            perror("sched_setaffinity failed. Try with sudo.");
            return 1;
        }
        printf("Process pinned to CPU %d\n", target_cpu);
    } else {
        printf("No CPU affinity set (running on all CPUs)\n");
    }
    
    /* --- ИЗМЕРЕНИЕ ПРОИЗВОДИТЕЛЬНОСТИ --- */
    long long latencies[NUM_ITERATIONS];
    long long min_latency = -1, max_latency = 0, total_latency = 0;
    
    printf("\nStarting benchmark (%d iterations)...\n", NUM_ITERATIONS);
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        struct timespec start, end;
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        matrix_multiply();
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        latencies[i] = timespec_diff_ns(start, end);
        
        if (min_latency == -1 || latencies[i] < min_latency) {
            min_latency = latencies[i];
        }
        if (latencies[i] > max_latency) {
            max_latency = latencies[i];
        }
        total_latency += latencies[i];
        
        // Прогресс каждые 10%
        if ((i + 1) % (NUM_ITERATIONS / 10) == 0) {
            printf("  Progress: %d%%\n", (i + 1) * 100 / NUM_ITERATIONS);
        }
    }
    
    double avg_latency = (double)total_latency / NUM_ITERATIONS;
    long long jitter = max_latency - min_latency;
    double stddev = calculate_stddev(latencies, avg_latency, NUM_ITERATIONS);
    
    /* --- ВЫВОД РЕЗУЛЬТАТОВ --- */
    printf("\n=== BENCHMARK RESULTS ===\n");
    printf("Configuration: %s\n", target_cpu != -1 ? "CPU Affinity ON" : "CPU Affinity OFF");
    printf("Scheduler: SCHED_FIFO (priority 50)\n");
    printf("Iterations: %d\n", NUM_ITERATIONS);
    printf("Matrix size: %dx%d\n", MATRIX_SIZE, MATRIX_SIZE);
    printf("\n--- Latency Statistics ---\n");
    printf("Min latency:        %12lld ns\n", min_latency);
    printf("Max latency:        %12lld ns\n", max_latency);
    printf("Avg latency:        %12.2f ns\n", avg_latency);
    printf("Jitter (max-min):   %12lld ns\n", jitter);
    printf("Standard deviation: %12.2f ns\n", stddev);
    printf("Jitter/Avg ratio:   %12.2f %%\n", (jitter * 100.0) / avg_latency);
    
    // Сохранение гистограммы
    printf("\n--- Distribution ---\n");
    long long bin_size = (max_latency - min_latency) / 10;
    if (bin_size > 0) {
        int bins[10] = {0};
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            int bin = (latencies[i] - min_latency) / bin_size;
            if (bin >= 10) bin = 9;
            bins[bin]++;
        }
        
        for (int i = 0; i < 10; i++) {
            long long range_start = min_latency + i * bin_size;
            long long range_end = min_latency + (i + 1) * bin_size;
            printf("%8lld-%-8lld ns: %4d |", range_start, range_end, bins[i]);
            for (int j = 0; j < bins[i] * 50 / NUM_ITERATIONS; j++) {
                printf("#");
            }
            printf("\n");
        }
    }
    
    return 0;
}