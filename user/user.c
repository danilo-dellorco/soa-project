#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int i;
char buff[4096];
#define DATA "driver test\n"
#define SIZE strlen(DATA)

void* the_thread(void* path) {
    char* device;
    int fd;

    device = (char*)path;
    sleep(1);

    printf("opening device %s\n", device);
    fd = open(device, O_RDWR);
    printf("open res fd: %d\n", fd);
    if (fd == -1) {
        printf("open error on device %s\n", device);
        return NULL;
    }
    printf("device %s successfully opened\n", device);
    ioctl(fd, 1);
    for (i = 0; i < 1000; i++) {
        write(fd, DATA, SIZE);
    }
    return NULL;
}

int main(int argc, char** argv) {
    int ret;
    int major;
    int minors;
    char* path;
    pthread_t tid;

    if (argc < 4) {
        printf("usage: sudo ./user DEVICE_PATH DRIVER_MAJOR MINORS\n");
        return -1;
    }

    path = argv[1];
    major = strtol(argv[2], NULL, 10);
    minors = strtol(argv[3], NULL, 10);
    printf("Creating %d minors for device %s with major %d\n", minors, path, major);

    for (i = 0; i < minors; i++) {
        sprintf(buff, "mknod %s%d c %d %i\n", path, i, major, i);
        system(buff);
        sprintf(buff, "%s%d", path, i);
        pthread_create(&tid, NULL, the_thread, strdup(buff));
    }

    pause();
    return 0;
}
