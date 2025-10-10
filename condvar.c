// ОСНОВНАЯ ЗАДАЧА: Реализовать автомат состояний с использованием 
// условных переменных для координации работы нескольких потоков.
// Каждый поток отвечает за одно состояние и ждет своего условия.

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>

// ЗАДАНИЕ: Реализовать 4-состояний автомат с переходами:
// 0 -> 1 -> 2 (если четно) -> 0
// 0 -> 1 -> 3 (если нечетно) -> 0
volatile int state;          // Текущее состояние автомата - разделяемый ресурс

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // Защита доступа к state
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;     // Условная переменная для ожидания

void *state_0(void *);
void *state_1(void *);
void *state_2(void *);
void *state_3(void *);
char *progname = "condvar";

int main() {
    setvbuf(stdout, NULL, _IOLBF, 0);
    state = 0;  // Начальное состояние автомата
    
    // ЗАДАНИЕ: Создать по одному потоку для каждого состояния
    pthread_t t_state0, t_state1, t_state2, t_state3;
    
    // Каждый поток реализует логику своего состояния
    pthread_create(&t_state0, NULL, state_0, NULL);  // Поток состояния 0
    pthread_create(&t_state1, NULL, state_1, NULL);  // Поток состояния 1  
    pthread_create(&t_state2, NULL, state_2, NULL);  // Поток состояния 2
    pthread_create(&t_state3, NULL, state_3, NULL);  // Поток состояния 3
    
    sleep(20);  // Дать автомату поработать
    
    // ЗАДАНИЕ: Корректно завершить все потоки автомата
    pthread_cancel(t_state0);
    pthread_cancel(t_state1);
    pthread_cancel(t_state2);
    pthread_cancel(t_state3);
    
    pthread_join(t_state0, NULL);
    pthread_join(t_state1, NULL);
    pthread_join(t_state2, NULL);
    pthread_join(t_state3, NULL);
    
    printf("%s: main, exiting\n", progname);
    return 0;
}

// ЗАДАНИЕ: Каждая функция состояния должна:
// 1. Ждать своего состояния под мьютексом
// 2. Выполнить работу состояния
// 3. Перейти в следующее состояние
// 4. Уведомить другие потоки

void *state_0(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        
        // ШАБЛОН ОЖИДАНИЯ: while (условие) pthread_cond_wait()
        // Защита от ложных пробуждений (spurious wakeups)
        while (state != 0) {  // Ждем, пока состояние не станет 0
            // АТОМАРНАЯ операция: разблокировка мьютекса + ожидание сигнала
            pthread_cond_wait(&cond, &mutex);
            // При пробуждении: автоматическая блокировка мьютекса
        }
        
        // ВЫПОЛНЕНИЕ РАБОТЫ СОСТОЯНИЯ 0
        printf("%s: state 0 -> transit to state 1\n", progname);
        state = 1;  // Переход в следующее состояние
        
        usleep(100 * 1000);  // Имитация работы состояния
        
        // УВЕДОМЛЕНИЕ: broadcast (а не signal) для пробуждения всех ожидающих
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void *state_1(void *arg) {
    static int counter = 0;  // Внутренняя переменная для логики переходов
    
    while (1) {
        pthread_mutex_lock(&mutex);
        while (state != 1) {
            pthread_cond_wait(&cond, &mutex);
        }
        
        // ЗАДАНИЕ: В состоянии 1 реализовать ветвление по четности
        counter++;
        printf("%s: state 1 - counter = %d\n", progname, counter);
        
        // ВЕТВЛЕНИЕ ПЕРЕХОДОВ: четность определяет следующее состояние
        if (counter % 2 == 0) {
            printf("%s: state 1 -> transit to state 2 (even)\n", progname);
            state = 2;  // Четный счетчик -> состояние 2
        } else {
            printf("%s: state 1 -> transit to state 3 (odd)\n", progname);
            state = 3;  // Нечетный счетчик -> состояние 3
        }
        
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Состояния 2 и 3 аналогичны - завершают цикл возвратом в состояние 0
void *state_2(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (state != 2) {
            pthread_cond_wait(&cond, &mutex);
        }
        
        printf("%s: state 2 -> transit to state 0\n", progname);
        state = 0;  // Завершение цикла: возврат в начальное состояние
        usleep(100 * 1000);
        
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void *state_3(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (state != 3) {
            pthread_cond_wait(&cond, &mutex);
        }
        
        printf("%s: state 3 -> transit to state 0\n", progname);
        state = 0;  // Альтернативный путь возврата в состояние 0
        usleep(100 * 1000);
        
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}
