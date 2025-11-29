/*
 *  Менеджер ресурсов (Linux версия)
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

#define EXAMPLE_SOCK_PATH "/tmp/example_resmgr.sock"
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

static const char *progname = "resmgr";
static int optv = 0;
static int listen_fd = -1;

// Структура для хранения состояния устройства
typedef struct {
    char buffer[BUFFER_SIZE];
    size_t position;
    size_t size;
    int write_protected;
    time_t last_access;
} device_t;

static device_t device = {0};
static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;

static void options(int argc, char *argv[]);
static void install_signals(void);
static void on_signal(int signo);
static void *client_thread(void *arg);
static void handle_command(int fd, const char *command);
static void send_response(int fd, const char *response);

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("%s: starting...\n", progname);
    options(argc, argv);
    install_signals();

    // Инициализация устройства
    pthread_mutex_lock(&device_mutex);
    device.position = 0;
    device.size = 0;
    device.write_protected = 0;
    device.last_access = time(NULL);
    pthread_mutex_unlock(&device_mutex);

    // Создаём UNIX-сокет и биндимся на путь
    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, EXAMPLE_SOCK_PATH, sizeof(addr.sun_path) - 1);

    // Удалим старый сокетный файл, если остался после прошлых запусков
    unlink(EXAMPLE_SOCK_PATH);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, MAX_CLIENTS) == -1) {
        perror("listen");
        close(listen_fd);
        unlink(EXAMPLE_SOCK_PATH);
        return EXIT_FAILURE;
    }

    printf("%s: listening on %s\n", progname, EXAMPLE_SOCK_PATH);
    printf("Подключитесь клиентом: `nc -U %s`\n", EXAMPLE_SOCK_PATH);
    printf("Доступные команды: READ, WRITE <text>, SEEK <pos>, STAT, PROTECT, UNPROTECT, HELP, QUIT\n");

    // Основной цикл accept
    while (1) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        if (optv) {
            printf("%s: новое подключение (fd=%d)\n", progname, client_fd);
        }

        pthread_t th;
        if (pthread_create(&th, NULL, client_thread, (void *)(long)client_fd) != 0) {
            perror("pthread_create");
            close(client_fd);
            continue;
        }
        pthread_detach(th);
    }

    if (listen_fd != -1) close(listen_fd);
    unlink(EXAMPLE_SOCK_PATH);
    pthread_mutex_destroy(&device_mutex);
    return EXIT_SUCCESS;
}

// Обработчик клиента
static void *client_thread(void *arg)
{
    int fd = (int)(long)arg;
    char buf[BUFFER_SIZE];
    
    // Приветственное сообщение
    const char *welcome = "Добро пожаловать в менеджер ресурсов!\n"
                         "Доступные команды: READ, WRITE <text>, SEEK <pos>, STAT, PROTECT, UNPROTECT, HELP, QUIT\n";
    send(fd, welcome, strlen(welcome), 0);

    while (1) {
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n == 0) {
            if (optv) printf("%s: клиент закрыл соединение (fd=%d)\n", progname, fd);
            break;
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            break;
        }

        buf[n] = '\0'; // Добавляем завершающий нуль
        
        if (optv) {
            printf("%s: получено %zd байт: %s\n", progname, n, buf);
        }

        // Обработка команды
        handle_command(fd, buf);
        
        // Проверяем команду QUIT
        if (strncasecmp(buf, "QUIT", 4) == 0) {
            break;
        }
    }

    close(fd);
    return NULL;
}

// Обработчик команд
static void handle_command(int fd, const char *command)
{
    char response[BUFFER_SIZE * 2];
    
    if (strncasecmp(command, "READ", 4) == 0) {
        pthread_mutex_lock(&device_mutex);
        if (device.size == 0) {
            strcpy(response, "Устройство пустое\n");
        } else {
            snprintf(response, sizeof(response), "Данные: %.*s\nПозиция: %zu/%zu\n",
                    (int)device.size, device.buffer, device.position, device.size);
        }
        device.last_access = time(NULL);
        pthread_mutex_unlock(&device_mutex);
    }
    else if (strncasecmp(command, "WRITE", 5) == 0) {
        pthread_mutex_lock(&device_mutex);
        if (device.write_protected) {
            strcpy(response, "ОШИБКА: Устройство защищено от записи\n");
        } else {
            const char *text = command + 5;
            while (*text == ' ') text++; // Пропускаем пробелы
            
            size_t len = strlen(text);
            if (len > BUFFER_SIZE - 1) len = BUFFER_SIZE - 1;
            
            memcpy(device.buffer, text, len);
            device.buffer[len] = '\0';
            device.size = len;
            device.position = 0;
            
            snprintf(response, sizeof(response), "Записано %zu байт\n", len);
        }
        device.last_access = time(NULL);
        pthread_mutex_unlock(&device_mutex);
    }
    else if (strncasecmp(command, "SEEK", 4) == 0) {
        pthread_mutex_lock(&device_mutex);
        const char *pos_str = command + 4;
        while (*pos_str == ' ') pos_str++;
        
        int pos = atoi(pos_str);
        if (pos < 0) pos = 0;
        if (pos > (int)device.size) pos = device.size;
        
        device.position = pos;
        snprintf(response, sizeof(response), "Позиция установлена на %d\n", pos);
        device.last_access = time(NULL);
        pthread_mutex_unlock(&device_mutex);
    }
    else if (strncasecmp(command, "STAT", 4) == 0) {
        pthread_mutex_lock(&device_mutex);
        time_t now = time(NULL);
        snprintf(response, sizeof(response), 
                "Размер: %zu байт\nПозиция: %zu\nЗащита: %s\nПоследний доступ: %s",
                device.size, device.position,
                device.write_protected ? "ВКЛ" : "ВЫКЛ",
                ctime(&device.last_access));
        device.last_access = now;
        pthread_mutex_unlock(&device_mutex);
    }
    else if (strncasecmp(command, "PROTECT", 7) == 0) {
        pthread_mutex_lock(&device_mutex);
        device.write_protected = 1;
        strcpy(response, "Защита записи включена\n");
        device.last_access = time(NULL);
        pthread_mutex_unlock(&device_mutex);
    }
    else if (strncasecmp(command, "UNPROTECT", 9) == 0) {
        pthread_mutex_lock(&device_mutex);
        device.write_protected = 0;
        strcpy(response, "Защита записи выключена\n");
        device.last_access = time(NULL);
        pthread_mutex_unlock(&device_mutex);
    }
    else if (strncasecmp(command, "HELP", 4) == 0) {
        strcpy(response, "Доступные команды:\n"
                        "READ - чтение данных\n"
                        "WRITE <text> - запись текста\n"
                        "SEEK <pos> - установка позиции\n"
                        "STAT - информация об устройстве\n"
                        "PROTECT - включить защиту записи\n"
                        "UNPROTECT - выключить защиту записи\n"
                        "HELP - эта справка\n"
                        "QUIT - выход\n");
    }
    else if (strncasecmp(command, "QUIT", 4) == 0) {
        strcpy(response, "До свидания!\n");
    }
    else {
        snprintf(response, sizeof(response), "Неизвестная команда: %s\n", command);
    }
    
    send_response(fd, response);
}

static void send_response(int fd, const char *response)
{
    ssize_t sent = 0;
    size_t len = strlen(response);
    
    while (sent < (ssize_t)len) {
        ssize_t n = send(fd, response + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("send");
            break;
        }
        sent += n;
    }
}

static void options(int argc, char *argv[])
{
    int opt;
    optv = 0;
    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                optv++;
                break;
        }
    }
}

static void install_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

static void on_signal(int signo)
{
    (void)signo;
    if (listen_fd != -1) close(listen_fd);
    unlink(EXAMPLE_SOCK_PATH);
    fprintf(stderr, "\n%s: завершение по сигналу\n", progname);
    _exit(0);
}