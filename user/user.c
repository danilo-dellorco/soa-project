#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int i;
char buff[4096];
int cmd;
int device_fd;
int minor;
#define DATA "driver test\n"
#define SIZE strlen(DATA)

#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_RESET "\x1b[0m"

#define RESET "\x1B[0m"
#define BOLD "\x1B[1m"

char* menu_list[] = {
    "|0  |  Open a device file",
    "|1  |  Write on the device file",
    "|2  |  Read from the device file",
    "|3  |  Switch to high priority flow",
    "|4  |  Switch to low priority flow",
    "|5  |  Make operations blocking",
    "|6  |  Make operations non blocking",
    "|7  |  Enable a device file",
    "|8  |  Disable a device file",
    "|9  |  Create device nodes",
    "|10 |  Delete all device nodes",
    "|-1 |  Exit",
};
int menu_size = sizeof(menu_list) / sizeof(char*);

char* device_path;
int show_menu();
int create_nodes();
int delete_nodes();
int write_op();
int read_op();
int open_device();
int exit_op();

int min(int num1, int num2) {
    if (num1 < num2)
        return num1;
    return num2;
}

int main(int argc, char** argv) {
    printf("\t\t\t\t\t%s%s| MultiFlow Device driver |%s%s\n\n", COLOR_YELLOW, BOLD, COLOR_RESET, RESET);

    if (argc < 2) {
        printf("usage: sudo ./user DEVICE_PATH\n");
        return -1;
    }
    device_path = argv[1];

    while (1) {
        show_menu();

        switch (cmd) {
            case (0):
                open_device();
                break;
            case (1):
                write_op();
                break;
            case (2):
                read_op();
                break;
            case (9):
                create_nodes();
                break;
            case (10):
                delete_nodes();
                break;
            case (-1):
                exit_op();
                break;
        }

        // pause();
    }
    return 0;
}

int open_device() {
    printf("\nInsert Minor Number of the device driver to open: ");
    scanf("%d", &minor);

    char dev[512];
    sprintf(dev, "%s%d", device_path, minor);
    printf("opening device %s\n", dev);
    device_fd = open(dev, O_RDWR);
    if (device_fd == -1) {
        printf("open error on device %s\n", dev);
        exit(-1);
    }
    printf("device %s successfully opened, fd is: %d\n", dev, device_fd);
}

int write_op() {
    char w_data[4096];
    int res;
    printf("Insert the data you want to write (max 4096): ");
    scanf("%s", w_data);

    res = write(device_fd, w_data, min(strlen(w_data), 4096));
    if (res == 0 || res == -1)
        printf(COLOR_RED "\nWrite result: could not write in the buffer \n" COLOR_RESET);
    else {
        printf(COLOR_GREEN "\nWrite result (%d bytes): operation completed successfully\n", res);
        printf(COLOR_RESET);
    }
}

int read_op() {
    printf("Insert the amount of data you want to read (max 4096): ");
    int amount;
    char r_data[4096];
    int res;
    scanf("%d", &amount);

    if (errno == ERANGE || errno == EINVAL)
        printf(COLOR_RED "\nRead: the amount of data inserted is not valid \n" COLOR_RESET);
    else {
        res = read(device_fd, r_data, min(amount, 4096));
        if (res == 0 || res == -1)
            printf(COLOR_RED "\nRead result: no data was read from the device file \n" COLOR_RESET);
        else {
            printf(COLOR_GREEN "\nRead result (%d bytes): ", res);
            printf(COLOR_RESET);
            printf("%s\n\n", r_data);
        }
    }
}

int show_menu() {
    for (i = 0; i < menu_size; ++i) {
        printf(COLOR_MAGENTA);
        printf("%s\n", menu_list[i]);
        printf(COLOR_RESET);
    }
    printf("\n\n Insert your command: ");
    scanf("%d", &cmd);
}

int create_nodes() {
    int minors;
    int major;

    printf("\nInsert Major Number of the device driver: ");
    scanf("%d", &major);
    printf("\nInsert Number of nodes to create: ");
    scanf("%d", &minors);

    printf("Creating %d minors for device %s with major %d\n", minors, device_path, major);

    for (i = 0; i < minors; i++) {
        sprintf(buff, "mknod %s%d c %d %i\n", device_path, i, major, i);
        system(buff);
        sprintf(buff, "%s%d", device_path, i);
        // pthread_create(&tid, NULL, the_thread, strdup(buff));
    }
}

int delete_nodes() {
    int minors;

    printf("\nInsert Number of nodes to delete: ");
    scanf("%d", &minors);

    printf("Deleting %d minors for device %s\n", minors, device_path);

    for (i = 0; i < minors; i++) {
        sprintf(buff, "rm %s%d\n", device_path, i);
        printf("%s", buff);
        system(buff);
    }
}

int exit_op() {
    // TODO aggiungere sblocco mutex rilascio risorse e quant'altro
    printf(COLOR_CYAN "Goodbye.\n" COLOR_RESET);
    exit(0);
}