// ОСНОВНАЯ ЗАДАЧА: Демонстрация обработки сигналов как аналога прерываний
// в пользовательском пространстве. Показать безопасные методы работы с сигналами.

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static const char *progname = "intsimple";

// ЗАДАНИЕ: Использовать volatile sig_atomic_t для обмена данными
// между обработчиками сигналов и основной программой
static volatile sig_atomic_t got_sigint = 0;   // Флаг для SIGINT (Ctrl+C)
static volatile sig_atomic_t got_sigterm = 0;  // Флаг для SIGTERM
static volatile sig_atomic_t got_sigusr1 = 0;  // Флаг для пользовательского сигнала 1
static volatile sig_atomic_t got_sigusr2 = 0;  // Флаг для пользовательского сигнала 2
static volatile sig_atomic_t got_sighup = 0;   // Флаг для SIGHUP

static struct termios orig_termios;

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

// ЗАДАНИЕ: Настроить неблокирующий ввод с клавиатуры через raw mode
static int enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return -1;
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);  // Отключить буферизацию и эхо
    raw.c_cc[VMIN] = 0;   // Неблокирующее чтение: минимум 0 символов
    raw.c_cc[VTIME] = 0;  // Таймаут 0 - возврат сразу
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1) return -1;
    atexit(restore_terminal);  // Автоматическое восстановление при выходе
    return 0;
}

// ЗАДАНИЕ: Обработчики сигналов должны быть МАКСИМАЛЬНО ПРОСТЫМИ
// и использовать только async-signal-safe функции
static void handle_sigint(int signo) { 
    (void)signo; 
    got_sigint = 1;  // Только установка флага - БЕЗОПАСНАЯ операция
    // НЕЛЬЗЯ: printf, malloc, pthread функции - они не async-signal-safe
}

static void handle_sigterm(int signo) { (void)signo; got_sigterm = 1; }
static void handle_sigusr1(int signo) { (void)signo; got_sigusr1 = 1; }
static void handle_sigusr2(int signo) { (void)signo; got_sigusr2 = 1; }
static void handle_sighup(int signo)  { (void)signo; got_sighup = 1; }

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("%s: starting...\n", progname);
    printf("Поддерживаемые сигналы: SIGINT(Ctrl+C), SIGTERM, SIGUSR1, SIGUSR2.\n");
    printf("Замечание: SIGKILL нельзя перехватить или обработать на Linux.\n");
    printf("Нажмите 'q' для выхода.\n");

    // ЗАДАНИЕ: Настроить терминал для неблокирующего ввода
    if (enable_raw_mode() == -1) {
        perror("termios");
        return EXIT_FAILURE;
    }

    // ЗАДАНИЕ: Установить обработчики для различных сигналов
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    // Регистрация обработчиков для разных типов сигналов
    sigaction(SIGINT, &sa, NULL);   // Ctrl+C
    sigaction(SIGTERM, &sa, NULL);  // Запрос завершения (kill)
    sigaction(SIGUSR1, &sa, NULL);  // Пользовательский сигнал 1
    sigaction(SIGUSR2, &sa, NULL);  // Пользовательский сигнал 2
    sigaction(SIGHUP, &sa, NULL);   // Hangup (закрытие терминала)

    // ЗАДАНИЕ: Использовать маски сигналов для защиты критических секций
    sigset_t blocked_set, old_set;
    sigemptyset(&blocked_set);
    sigaddset(&blocked_set, SIGINT);
    sigaddset(&blocked_set, SIGTERM);
    sigaddset(&blocked_set, SIGUSR1);
    sigaddset(&blocked_set, SIGUSR2);
    sigaddset(&blocked_set, SIGHUP);

    // ОСНОВНОЙ ЦИКЛ ОБРАБОТКИ: опрос флагов и неблокирующий ввод
    for (;;) {
        // ЗАЩИТА КРИТИЧЕСКОЙ СЕКЦИИ: блокировка сигналов при проверке флагов
        sigprocmask(SIG_BLOCK, &blocked_set, &old_set);
        
        // Проверка флагов сигналов (под защитой от прерываний)
        if (got_sigint)  { got_sigint = 0;  printf("%s: получен SIGINT (Ctrl+C)\n", progname); }
        if (got_sigterm) { got_sigterm = 0; printf("%s: получен SIGTERM\n", progname); }
        if (got_sigusr1) { got_sigusr1 = 0; printf("%s: получен SIGUSR1\n", progname); }
        if (got_sigusr2) { got_sigusr2 = 0; printf("%s: получен SIGUSR2\n", progname); }
        if (got_sighup)  { got_sighup = 0;  printf("%s: получен SIGHUP\n", progname); }
        
        sigprocmask(SIG_SETMASK, &old_set, NULL);  // Разблокировка сигналов

        // ЗАДАНИЕ: Неблокирующее чтение клавиатуры
        char ch;
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        if (n == 1) {
            if (ch == 'q' || ch == 'Q') {
                printf("%s: выход по клавише 'q'\n", progname);
                break;
            }
            // Обработка других команд...
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read");
            break;
        }

        usleep(10 * 1000);  // Небольшая пауза для снижения нагрузки на CPU
    }

    printf("%s: exiting...\n", progname);
    return EXIT_SUCCESS;
}
