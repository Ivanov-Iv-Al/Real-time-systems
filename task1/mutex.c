// ОСНОВНАЯ ЗАДАЧА: Продемонстрировать проблему гонки данных (race condition)
// и её решение с помощью мьютексов.
// Без мьютексов: var1 и var2 рассинхронизируются.
// С мьютексами: var1 всегда равен var2.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#define NumThreads 4       // ЗАДАНИЕ: Создать несколько конкурентных потоков

volatile int var1;         // РАЗДЕЛЯЕМЫЙ РЕСУРС 1 - требует синхронизации
volatile int var2;         // РАЗДЕЛЯЕМЫЙ РЕСУРС 2 - должен быть равен var1
pthread_mutex_t mutex;     // МЬЮТЕКС - основной механизм синхронизации

void *update_thread(void *);
char *progname = "mutex";

int main() {
    pthread_t threadID[NumThreads];
    pthread_attr_t attrib;
    struct sched_param param;
    int i, policy;
    
    // ЗАДАНИЕ: Инициализировать мьютекс для защиты критических секций
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("%s: ошибка инициализации мьютекса\n", progname);
        exit(1);
    }
    
    setvbuf(stdout, NULL, _IOLBF, 0);
    var1 = var2 = 0;        // Инициализация разделяемых ресурсов
    printf("%s: starting; creating threads\n", progname);
    
    // ЗАДАНИЕ: Настроить планирование потоков для лучшего наблюдения эффектов
    pthread_getschedparam(pthread_self(), &policy, &param);
    pthread_attr_init(&attrib);
    pthread_attr_setinheritsched(&attrib, PTHREAD_EXPLICIT_SCHED); // Явное управление
    pthread_attr_setschedpolicy(&attrib, SCHED_RR);                // Round Robin
    param.sched_priority -= 2;        // Снизить приоритет рабочих потоков
    pthread_attr_setschedparam(&attrib, &param);

    // ЗАДАНИЕ: Создать несколько потоков, конкурирующих за общие ресурсы
    for (i = 0; i < NumThreads; i++) {
        pthread_create(&threadID[i], &attrib, &update_thread, (void *)(long)i);
    }

    sleep(20);  // Дать потокам время поработать и проявить проблемы синхронизации
    
    // ЗАДАНИЕ: Корректно завершить потоки и проверить итоговое состояние
    printf("%s: stopping; cancelling threads\n", progname);
    for (i = 0; i < NumThreads; i++) {
        pthread_cancel(threadID[i]);  // Принудительное завершение
    }
    
    // Ожидание фактического завершения потоков
    for (i = 0; i < NumThreads; i++) {
        pthread_join(threadID[i], NULL);
    }
    
    pthread_mutex_destroy(&mutex);  // Освобождение ресурсов мьютекса
    
    // ЗАДАНИЕ: Показать итоговые значения - должны быть равны при корректной синхронизации
    printf("%s: all done, var1 is %d, var2 is %d\n", progname, var1, var2);
    exit(0);
}

void do_work() {
    static int var3;
    var3++;
    // Имитация работы для увеличения вероятности переключения контекста
    if (!(var3 % 10000000)) 
        printf("%s: thread %lu did some work\n", progname, (unsigned long)pthread_self());
}

void *update_thread(void *i) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    
    // ЗАДАНИЕ: В бесконечном цикле демонстрировать проблему и её решение
    while (1) {
        // КРИТИЧЕСКАЯ СЕКЦИЯ - начинается с захвата мьютекса
        pthread_mutex_lock(&mutex);  // ЗАЩИТА: только один поток выполняет дальше
        
        // ПРОВЕРКА ЦЕЛОСТНОСТИ: var1 должен равняться var2
        if (var1 != var2) {
            // БЕЗ МЬЮТЕКСА: это сообщение будет появляться часто
            printf("%s: thread %ld, var1 (%d) is not equal to var2 (%d)!\n",
                    progname, (long)i, var1, var2);
            var1 = var2;  // Принудительное восстановление целостности
        }
        
        do_work();  // Имитация работы (может вызвать переключение контекста)
        
        // ИЗМЕНЕНИЕ РАЗДЕЛЯЕМЫХ РЕСУРСОВ - потенциальная точка гонки данных
        var1++;  // Операция не атомарна: чтение-изменение-запись
        // МЕЖДУ var1++ и var2++ может произойти переключение на другой поток!
        var2++;  // Без мьютекса это приводит к рассинхронизации
        
        pthread_mutex_unlock(&mutex);  // Конец критической секции
    }
    return NULL;
}
