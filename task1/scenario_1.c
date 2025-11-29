#include "working.h"
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>

static int set_thread_priority(pthread_attr_t *attr, int policy, int prio) {
    pthread_attr_init(attr);
    pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(attr, policy);
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = prio;
    return pthread_attr_setschedparam(attr, &sp);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    const int policy = SCHED_FIFO;
    const int prio_server = 10;
    const int prio_t1 = 20;  
    const int prio_t2 = 30;

    // ЗАДАНИЕ: Инициализировать мьютекс С наследованием приоритетов
    // Это РЕШЕНИЕ проблемы инверсии приоритетов
    if (init_resource_mutex(1) != 0) {
        perror("init_resource_mutex");
        printf("Наследование приоритетов не поддерживается, используем обычный мьютекс\n");
        // Fallback: обычный мьютекс (проблема останется)
        if (init_resource_mutex(0) != 0) {
            perror("init_resource_mutex (fallback)");
            return EXIT_FAILURE;
        }
    }

    // Та же настройка потоков, что и в scenario_1
    pthread_attr_t attr_server, attr_t1, attr_t2;
    set_thread_priority(&attr_server, policy, prio_server);
    set_thread_priority(&attr_t1, policy, prio_t1);
    set_thread_priority(&attr_t2, policy, prio_t2);

    pthread_t th_server, th_t1, th_t2;

    // Тот же сценарий, но с РЕШЕНИЕМ:
    pthread_create(&th_server, &attr_server, server, NULL);
    usleep(50 * 1000);
    pthread_create(&th_t2, &attr_t2, t2, NULL);
    pthread_create(&th_t1, &attr_t1, t1, NULL);

    // МЕХАНИЗМ РЕШЕНИЯ (PTHREAD_PRIO_INHERIT):
    // 1. Когда t2 (приоритет 30) блокируется на мьютексе server'а
    // 2. Система ВРЕМЕННО повышает приоритет server до 30
    // 3. Server быстро завершает работу и освобождает мьютекс
    // 4. t2 немедленно получает мьютекс и выполняется
    // 5. Приоритет server возвращается к 10

    void *st;
    pthread_join(th_t1, &st);
    pthread_join(th_t2, &st);
    pthread_join(th_server, &st);

    printf("scenario_2: завершено. Наследование приоритетов предотвратило инверсию.\n");
    printf("Высокоприоритетный поток t2 должен был выполниться быстрее, несмотря на средний приоритет t1.\n");
    return EXIT_SUCCESS;
}
