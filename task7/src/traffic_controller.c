#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "common.h"

// Глобальные переменные
SharedData shared_data;
timer_t timer;
volatile sig_atomic_t timer_expired = 0;
volatile sig_atomic_t emergency_active = 0;

// Флаг для выхода из программы
volatile sig_atomic_t program_running = 1;

// Обработчик сигнала от таймера
void timer_handler(int sig) {
    timer_expired = 1;
}

// Обработчик Ctrl+C для корректного завершения
void sigint_handler(int sig) {
    program_running = 0;
}

// Функция для установки таймера
void set_timer(int seconds) {
    struct itimerspec its;
    its.it_value.tv_sec = seconds;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    
    if (timer_settime(timer, 0, &its, NULL) == -1) {
        perror("timer_settime failed");
    }
}

// Функция для красивого вывода текущего состояния светофоров
void print_lights(TrafficState state) {
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    char time_str[9];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    printf("[%s] ", time_str);
    
    switch (state) {
        case STATE_INIT:
            printf("Инициализация системы\n");
            break;
        case STATE_NS_GREEN:
            printf("Север-Юг: ЗЕЛЕНЫЙ | Запад-Восток: КРАСНЫЙ\033[0m\n");
            break;
        case STATE_NS_YELLOW:
            printf("Север-Юг: ЖЕЛТЫЙ | Запад-Восток: РАСНЫЙ\033[0m\n");
            break;
        case STATE_EW_GREEN:
            printf("Север-Юг: КРАСНЫЙ | Запад-Восток: ЗЕЛЕНЫЙ\033[0m\n");
            break;
        case STATE_EW_YELLOW:
            printf("Север-Юг: КРАСНЫЙ | Запад-Восток: ЖЕЛТЫЙ\033[0m\n");
            break;
        case STATE_ALL_RED:
            printf("Север-Юг: КРАСНЫЙ | Запад-Восток: КРАСНЫЙ\033[0m\n");
            break;
        case STATE_PED_CROSS:
            printf("ВСЕМ КРАСНЫЙ | ПЕШЕХОДЫ ИДУТ\033[0m\n");
            break;
        case STATE_EMERGENCY:
            printf("РЕЖИМ ЧРЕЗВЫЧАЙНОЙ СИТУАЦИИ \n");
            break;
        default:
            printf("Неизвестное состояние: %d\n", state);
            break;
    }
}

// Функция проверки запросов пешеходов
int check_pedestrian_requests() {
    if (shared_data.ped_ns_request || shared_data.ped_ew_request) {
        return 1;
    }
    return 0;
}

// Функция сброса запросов пешеходов
void reset_pedestrian_requests() {
    shared_data.ped_ns_request = 0;
    shared_data.ped_ew_request = 0;
}

// Функция потока контроллера (FSM)
void* controller_thread_func(void* arg) {
    TrafficState next_state = STATE_ALL_RED;
    int was_in_emergency = 0;
    
    // Начальная инициализация
    pthread_mutex_lock(&shared_data.mutex);
    shared_data.current_state = STATE_INIT;
    print_lights(shared_data.current_state);
    pthread_mutex_unlock(&shared_data.mutex);
    
    sleep(1); // Краткая пауза для инициализации
    
    while (program_running) {
        // Проверка режима ЧС
        pthread_mutex_lock(&shared_data.mutex);
        if (shared_data.emergency_request) {
            emergency_active = !emergency_active;
            shared_data.emergency_request = 0;
            
            if (emergency_active) {
                next_state = STATE_EMERGENCY;
                was_in_emergency = 1;
            } else {
                // Выход из режима ЧС
                next_state = STATE_ALL_RED;
            }
        }
        pthread_mutex_unlock(&shared_data.mutex);
        
        // Если активен режим ЧС
        if (emergency_active) {
            pthread_mutex_lock(&shared_data.mutex);
            shared_data.current_state = STATE_EMERGENCY;
            print_lights(shared_data.current_state);
            pthread_mutex_unlock(&shared_data.mutex);
            
            // Мигаем красным в режиме ЧС
            while (emergency_active && program_running) {
                print_lights(STATE_EMERGENCY);
                sleep(1);
                pthread_mutex_lock(&shared_data.mutex);
                if (shared_data.emergency_request) {
                    emergency_active = !emergency_active;
                    shared_data.emergency_request = 0;
                }
                pthread_mutex_unlock(&shared_data.mutex);
            }
            continue;
        }
        
        // Нормальная работа FSM
        pthread_mutex_lock(&shared_data.mutex);
        shared_data.current_state = next_state;
        print_lights(shared_data.current_state);
        
        // Если вышли из режима ЧС, сбрасываем все запросы
        if (was_in_emergency) {
            reset_pedestrian_requests();
            was_in_emergency = 0;
        }
        pthread_mutex_unlock(&shared_data.mutex);
        
        timer_expired = 0;
        int timer_duration = 1; // По умолчанию
        
        // Логика конечного автомата
        switch (next_state) {
            case STATE_ALL_RED:
                timer_duration = ALL_RED_DURATION;
                // Проверяем запросы пешеходов перед выбором следующего состояния
                pthread_mutex_lock(&shared_data.mutex);
                if (check_pedestrian_requests()) {
                    next_state = STATE_PED_CROSS;
                } else {
                    next_state = STATE_NS_GREEN;
                }
                pthread_mutex_unlock(&shared_data.mutex);
                break;
                
            case STATE_NS_GREEN:
                timer_duration = GREEN_DURATION;
                next_state = STATE_NS_YELLOW;
                break;
                
            case STATE_NS_YELLOW:
                timer_duration = YELLOW_DURATION;
                next_state = STATE_ALL_RED;
                break;
                
            case STATE_EW_GREEN:
                timer_duration = GREEN_DURATION;
                next_state = STATE_EW_YELLOW;
                break;
                
            case STATE_EW_YELLOW:
                timer_duration = YELLOW_DURATION;
                next_state = STATE_ALL_RED;
                break;
                
            case STATE_PED_CROSS:
                timer_duration = PED_CROSS_DURATION;
                pthread_mutex_lock(&shared_data.mutex);
                reset_pedestrian_requests();
                pthread_mutex_unlock(&shared_data.mutex);
                
                // После пешеходного перехода возвращаемся к нормальному циклу
                next_state = STATE_ALL_RED;
                break;
                
            case STATE_EMERGENCY:
                timer_duration = 1;
                next_state = STATE_EMERGENCY;
                break;
                
            default:
                timer_duration = 1;
                next_state = STATE_ALL_RED;
                break;
        }
        
        // Взводим таймер
        set_timer(timer_duration);
        
        // Ожидаем истечения таймера с возможностью прерывания
        while (!timer_expired && program_running) {
            // Проверяем режим ЧС каждые 100ms
            usleep(100000);
            
            pthread_mutex_lock(&shared_data.mutex);
            if (shared_data.emergency_request) {
                // Немедленный переход в режим ЧС
                emergency_active = 1;
                shared_data.emergency_request = 0;
                timer_expired = 1; // Прерываем ожидание
            }
            pthread_mutex_unlock(&shared_data.mutex);
        }
    }
    
    return NULL;
}

// Функция потока для пользовательского ввода
void* input_thread_func(void* arg) {
    printf("\n=== Управление перекрестком ===\n");
    printf("Клавиши управления:\n");
    printf("  n - Запрос пешехода Север-Юг\n");
    printf("  e - Запрос пешехода Запад-Восток\n");
    printf("  s - Включить/выключить режим ЧС\n");
    printf("  q - Выход из программы\n");
    
    while (program_running) {
        char c = getchar();
        
        if (c == 'q' || c == 'Q') {
            program_running = 0;
            break;
        }
        
        pthread_mutex_lock(&shared_data.mutex);
        
        switch (c) {
            case 'n':
            case 'N':
                if (!emergency_active) {
                    shared_data.ped_ns_request = 1;
                    printf("Запрос пешехода Север-Юг зарегистрирован\n");
                }
                break;
                
            case 'e':
            case 'E':
                if (!emergency_active) {
                    shared_data.ped_ew_request = 1;
                    printf("Запрос пешехода Запад-Восток зарегистрирован\n");
                }
                break;
                
            case 's':
            case 'S':
                shared_data.emergency_request = 1;
                if (emergency_active) {
                    printf("Режим ЧС отключен\n");
                } else {
                    printf("АКТИВИРОВАН РЕЖИМ ЧС!\n");
                }
                break;
                
            case '\n': // Игнорируем Enter
                break;
                
            default:
                if (c != EOF) {
                    printf("Неизвестная команда: '%c'\n", c);
                }
                break;
        }
        
        pthread_mutex_unlock(&shared_data.mutex);
        
        // Очищаем буфер ввода
        if (c != '\n' && c != EOF) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
        }
    }
    
    return NULL;
}

int main() {
    // Инициализация разделяемых данных
    memset(&shared_data, 0, sizeof(SharedData));
    pthread_mutex_init(&shared_data.mutex, NULL);
    shared_data.current_state = STATE_INIT;
    
    // Настройка обработчика Ctrl+C
    struct sigaction sa_int;
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);
    
    // Настройка обработчика сигнала для таймера
    struct sigaction sa;
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGRTMIN, &sa, NULL);
    
    // Создание POSIX таймера
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timer;
    
    if (timer_create(CLOCK_REALTIME, &sev, &timer) == -1) {
        perror("timer_create failed");
        return 1;
    }
    
    // Создание потоков
    pthread_t controller_thread, input_thread;
    
    printf("🚦 Запуск системы управления перекрестком...\n");
    
    if (pthread_create(&controller_thread, NULL, controller_thread_func, NULL) != 0) {
        perror("Failed to create controller thread");
        return 1;
    }
    
    if (pthread_create(&input_thread, NULL, input_thread_func, NULL) != 0) {
        perror("Failed to create input thread");
        return 1;
    }
    
    // Ожидание завершения потоков
    pthread_join(controller_thread, NULL);
    pthread_join(input_thread, NULL);
    
    // Завершение работы
    printf("\nЗавершение работы системы...\n");
    
    // Уничтожение мьютекса и таймера
    pthread_mutex_destroy(&shared_data.mutex);
    timer_delete(timer);
    
    printf("Система остановлена корректно.\n");
    
    return 0;
}