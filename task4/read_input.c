#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s /dev/input/eventX\n", argv[0]);
        return 1;
    }

    const char *device_path = argv[1];

    // Открыть файл устройства для чтения (O_RDONLY)
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    /*Использование IOCTL*/
    char device_name[256] = "Unknown";
    char device_phys[256] = "Unknown";
    
    // Получаем имя устройства
    if (ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name) < 0) {
        perror("Failed to get device name");
    }
    
    // Получаем физический путь устройства
    if (ioctl(fd, EVIOCGPHYS(sizeof(device_phys)), device_phys) < 0) {
        perror("Failed to get device physical path");
    }
    
    printf("Device name: %s\n", device_name);
    printf("Physical path: %s\n", device_phys);
    printf("Reading events from %s. Press Ctrl+C to exit.\n", device_path);

    struct input_event ev;
    while (1) {
        // Прочитать структуру input_event из файла устройства
        ssize_t bytes = read(fd, &ev, sizeof(struct input_event));
        if (bytes != sizeof(struct input_event)) {
            perror("Failed to read event");
            break;
        }

        // Выводим информацию о событии
        printf("Event: time %ld.%06ld, type %d, code %d, value %d\n",
               ev.time.tv_sec, ev.time.tv_usec, ev.type, ev.code, ev.value);
    }

    close(fd);
    return 0;
}