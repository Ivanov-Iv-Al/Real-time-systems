#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <string.h>

#define MAX_DEVICES 16

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s /dev/input/eventX1 /dev/input/eventX2 ...\n", argv[0]);
        return 1;
    }

    if (argc - 1 > MAX_DEVICES) {
        fprintf(stderr, "Error: Too many devices. Max is %d.\n", MAX_DEVICES);
        return 1;
    }

    int num_devices = argc - 1;
    struct pollfd fds[MAX_DEVICES];
    char device_names[MAX_DEVICES][256];
    char device_phys[MAX_DEVICES][256];

    // Открыть все переданные устройства
    for (int i = 0; i < num_devices; i++) {
        const char *path = argv[i + 1];
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            perror("Failed to open device");
            // Закрываем уже открытые дескрипторы перед выходом
            for (int j = 0; j < i; j++) {
                close(fds[j].fd);
            }
            return 1;
        }

        fds[i].fd = fd;
        fds[i].events = POLLIN;
        fds[i].revents = 0;

        // Получаем имя и физический путь устройства
        if (ioctl(fd, EVIOCGNAME(sizeof(device_names[i])), device_names[i]) < 0) {
            strcpy(device_names[i], "Unknown");
        }
        if (ioctl(fd, EVIOCGPHYS(sizeof(device_phys[i])), device_phys[i]) < 0) {
            strcpy(device_phys[i], "Unknown");
        }
        
        printf("Added device: %s (%s)\n", device_names[i], device_phys[i]);
    }

    printf("\nPolling %d devices. Press Ctrl+C to exit.\n", num_devices);

    while (1) {
        // Вызвать poll() для ожидания событий
        int ret = poll(fds, num_devices, -1);
        if (ret < 0) {
            perror("poll failed");
            break;
        }

        // Проверить, на каких файловых дескрипторах произошли события
        for (int i = 0; i < num_devices; i++) {
            if (fds[i].revents & POLLIN) {
                struct input_event ev;
                
                // Читаем все доступные события (из-за O_NONBLOCK)
                while (read(fds[i].fd, &ev, sizeof(ev)) == sizeof(ev)) {
                    printf("Event from %s: type %d, code %d, value %d\n",
                           device_names[i], ev.type, ev.code, ev.value);
                }
            }
            
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                fprintf(stderr, "Error on device %s\n", device_names[i]);
            }
        }
    }

    // Закрыть все файловые дескрипторы
    for (int i = 0; i < num_devices; i++) {
        close(fds[i].fd);
    }

    return 0;
}