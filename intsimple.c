//  Демонстрация обработки "прерываний" на Linux: сигналы и ввод с клавиатуры.

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static const char *progname = "intsimple";

// Флаги сигналов (устанавливаются в обработчиках, читаются в основном цикле)
static volatile sig_atomic_t got_sigint = 0;
static volatile sig_atomic_t got_sigterm = 0;
static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigusr2 = 0;
static volatile sig_atomic_t got_sighup = 0;

static struct termios orig_termios;

static void restore_terminal(void) {
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

static int enable_raw_mode(void) {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return -1;
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN] = 0;   // неблокирующее чтение
  raw.c_cc[VTIME] = 0;  // без таймаута
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1) return -1;
  atexit(restore_terminal);
  return 0;
}

static void handle_sigint(int signo) { 
    (void)signo; 
    got_sigint = 1; 
    printf("\n%s: SIGINT получен (установлен флаг)\n", progname);
}

static void handle_sigterm(int signo) { 
    (void)signo; 
    got_sigterm = 1; 
    printf("\n%s: SIGTERM получен (установлен флаг)\n", progname);
}

static void handle_sigusr1(int signo) { 
    (void)signo; 
    got_sigusr1 = 1; 
    printf("\n%s: SIGUSR1 получен (установлен флаг)\n", progname);
}

static void handle_sigusr2(int signo) { 
    (void)signo; 
    got_sigusr2 = 1; 
    printf("\n%s: SIGUSR2 получен (установлен флаг)\n", progname);
}

static void handle_sighup(int signo) { 
    (void)signo; 
    got_sighup = 1; 
    printf("\n%s: SIGHUP получен (установлен флаг)\n", progname);
}

static void print_help(void) {
    printf("\n%s: Доступные команды:\n", progname);
    printf("  q - выход\n");
    printf("  h - помощь\n");
    printf("  s - отправить SIGUSR1 самому себе\n");
    printf("  r - сбросить все флаги сигналов\n");
    printf("  p - показать текущие флаги сигналов\n");
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("%s: starting...\n", progname);
    printf("Поддерживаемые сигналы: SIGINT(Ctrl+C), SIGTERM, SIGUSR1, SIGUSR2, SIGHUP.\n");
    printf("Замечание: SIGKILL нельзя перехватить или обработать на Linux.\n");
    printf("Нажмите 'h' для помощи.\n");

    if (enable_raw_mode() == -1) {
        perror("termios");
        return EXIT_FAILURE;
    }

    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint; 
    sigemptyset(&sa.sa_mask); 
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    sa.sa_handler = handle_sigterm; 
    sigaction(SIGTERM, &sa, NULL);
    
    sa.sa_handler = handle_sigusr1; 
    sigaction(SIGUSR1, &sa, NULL);
    
    sa.sa_handler = handle_sigusr2; 
    sigaction(SIGUSR2, &sa, NULL);
    
    sa.sa_handler = handle_sighup; 
    sigaction(SIGHUP, &sa, NULL);

    // Блокируем сигналы во время обработки
    sigset_t blocked_set, old_set;
    sigemptyset(&blocked_set);
    sigaddset(&blocked_set, SIGINT);
    sigaddset(&blocked_set, SIGTERM);
    sigaddset(&blocked_set, SIGUSR1);
    sigaddset(&blocked_set, SIGUSR2);
    sigaddset(&blocked_set, SIGHUP);

    // Основной цикл: опрашиваем stdin и проверяем флаги сигналов
    for (;;) {
        // Блокируем сигналы на время проверки флагов
        sigprocmask(SIG_BLOCK, &blocked_set, &old_set);
        
        // Проверка сигналов
        if (got_sigint)  { 
            got_sigint = 0;  
            printf("%s: обработан SIGINT (Ctrl+C)\n", progname); 
        }
        if (got_sigterm) { 
            got_sigterm = 0; 
            printf("%s: обработан SIGTERM\n", progname); 
        }
        if (got_sigusr1) { 
            got_sigusr1 = 0; 
            printf("%s: обработан SIGUSR1\n", progname); 
        }
        if (got_sigusr2) { 
            got_sigusr2 = 0; 
            printf("%s: обработан SIGUSR2\n", progname); 
        }
        if (got_sighup)  { 
            got_sighup = 0;  
            printf("%s: обработан SIGHUP\n", progname); 
        }
        
        // Разблокируем сигналы
        sigprocmask(SIG_SETMASK, &old_set, NULL);

        // Неблокирующее чтение клавиатуры
        char ch;
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        if (n == 1) {
            if (ch == 'q' || ch == 'Q') {
                printf("%s: выход по клавише 'q'\n", progname);
                break;
            }
            else if (ch == 'h' || ch == 'H') {
                print_help();
            }
            else if (ch == 's' || ch == 'S') {
                printf("%s: отправляем SIGUSR1 самому себе\n", progname);
                raise(SIGUSR1);
            }
            else if (ch == 'r' || ch == 'R') {
                got_sigint = got_sigterm = got_sigusr1 = got_sigusr2 = got_sighup = 0;
                printf("%s: все флаги сигналов сброшены\n", progname);
            }
            else if (ch == 'p' || ch == 'P') {
                printf("%s: текущие флаги - SIGINT:%d SIGTERM:%d SIGUSR1:%d SIGUSR2:%d SIGHUP:%d\n",
                       progname, got_sigint, got_sigterm, got_sigusr1, got_sigusr2, got_sighup);
            }
            else if (ch == '\n' || ch == '\r') {
                // игнорировать переводы строк
            } else {
                printf("%s: клавиша '%c' (нажмите 'h' для помощи)\n", progname, ch);
            }
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("read");
            break;
        }

        // Немного подождём, чтобы не крутить CPU
        usleep(10 * 1000);
    }

    printf("%s: exiting...\n", progname);
    return EXIT_SUCCESS;
}