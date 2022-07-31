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
#define NUM_DEVICES 3  // TODO set to 128

int i;
char data_buff[4096];
int cmd;
int device_fd = -1;
int minor;
int major;
char* device_path;
char opened_device[64] = "none";
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

char* main_menu_list[] = {
    "--------- OPERATIONS ---------",
    "0)  Open a device file",
    "1)  Write on the device file",
    "2)  Read from the device file",
    "",
    "------ SESSION SETTINGS ------",
    "3)  Switch to HIGH priority",
    "4)  Switch to LOW priority",
    "5)  Use BLOCKING operations",
    "6)  Use NON-BLOCKING operations",
    "7)  Set timeout",
    "",
    "------ DEVICE MANAGEMENT ------",
    "8)  Enable a device file",
    "9)  Disable a device file",
    "10) See device status",
    "11) Create device nodes",
    "",
    "-------------------------------",
    "-1) Exit",
};
int menu_size = sizeof(main_menu_list) / sizeof(char*);

int show_menu();
int create_nodes();
int delete_nodes();
int write_op();
int read_op();
int open_device();
int exit_op();
int set_enabling(int minor, int status);

int min(int num1, int num2) {
    if (num1 < num2)
        return num1;
    return num2;
}

void myflush(FILE* in) {
    int ch;
    do
        ch = fgetc(in);
    while (ch != EOF && ch != '\n');
    clearerr(in);
}

void wait_input(void) {
    // myflush(stdin);
    printf("───────────────────────────────────────────────────────────\n");
    printf("Press %s[Enter]%s to continue...", BOLD, RESET);
    fflush(stdout);
    getchar();
}

void clear_buffer() {
    memset(data_buff, 0, 4096);
}

int main(int argc, char** argv) {
    int ret;
    if (argc < 3) {
        printf("usage: sudo ./user DEVICE_PATH MAJOR\n");
        return -1;
    }
    device_path = argv[1];
    major = atoi(argv[2]);

    while (1) {
        show_menu();
        switch (cmd) {
            case (0):
                open_device();
                wait_input();
                break;
            case (1):
                write_op();
                wait_input();
                break;
            case (2):
                read_op();
                wait_input();
                break;
            case (3):
                printf("'switch to high priority' not implemented yet.\n");
                wait_input();
                break;
            case (4):
                printf("'switch to low priority' not implemented yet.\n");
                wait_input();
                break;
            case (5):
                printf("'use blocking operations' not implemented yet.\n");
                wait_input();
                break;
            case (6):
                printf("'use non-blocking operations' not implemented yet.\n");
                wait_input();
                break;
            case (7):
                printf("'set timeout' not implemented yet.\n");
                wait_input();
                break;
            case (8):
                int minor_en;
                printf("Enter the minor number of the device to enable (-1 for currently opened device): ");
                fgets(data_buff, sizeof(data_buff), stdin);
                minor_en = atoi(data_buff);
                if (minor_en == -1) {
                    if (device_fd == -1) {
                        printf(COLOR_RED "Open a device first.\n" COLOR_RESET);
                        wait_input();
                        break;
                    }
                    minor_en = minor;
                }
                clear_buffer();
                set_enabling(minor_en, 1);
                // ioctl(device_fd, 8, NULL);
                wait_input();
                break;
            case (9):
                printf("'disable device file' not implemented yet.\n");
                ioctl(device_fd, 9, NULL);
                wait_input();
                break;
            case (10):
                printf("'see device status' not implemented yet.\n");
                wait_input();
                break;
            case (11):
                create_nodes();
                wait_input();
                break;
            case (-1):
                exit_op();
                break;
            default:
                printf(COLOR_RED "Insert a valid command from the list\n" COLOR_RESET);
                wait_input();
                break;
        }

        // pause();
    }
    return 0;
}

int open_device() {
    printf("Insert Minor Number of the device driver to open: ");
    fgets(data_buff, sizeof(data_buff), stdin);

    if (atoi(data_buff) < 0 || atoi(data_buff) > 127) {
        printf("Insert a valid minor, between 0 and 127");
        return (-1);
    }
    minor = atoi(data_buff);
    clear_buffer();

    if (device_fd != -1) {
        printf("device %s is already opened. Closing.\n", opened_device);
        close(device_fd);
    }

    sprintf(opened_device, "%s%d", device_path, minor);
    printf("opening device %s\n", opened_device);
    device_fd = open(opened_device, O_RDWR);
    if (device_fd == -1) {
        printf("open error on device %s\n", opened_device);
        sprintf(opened_device, "none");
        device_fd = -1;
        return -1;
    }
    if (device_fd == -2) {
        printf("the device file %s is disabled, and cannot be opened\n", opened_device);
        sprintf(opened_device, "none");
        device_fd = -1;
        return -1;
    }
    printf("device %s successfully opened, fd is: %d\n", opened_device, device_fd);
}

int write_op() {
    if (device_fd < 0) {
        printf(COLOR_RED "Open a device first.\n" COLOR_RESET);
        return -1;
    }

    int res;
    clear_buffer();
    printf("Insert the data you want to write (max 4096): ");
    fgets(data_buff, sizeof(data_buff), stdin);
    data_buff[strcspn(data_buff, "\n")] = 0;  // ignoring \n

    res = write(device_fd, data_buff, min(strlen(data_buff), 4096));
    if (res == 0 || res == -1)
        printf(COLOR_RED "\nWrite result: could not write in the buffer \n" COLOR_RESET);
    else {
        printf(COLOR_GREEN "\nWrite result (%d bytes): operation completed successfully\n", res);
        printf(COLOR_RESET);
    }
}

int read_op() {
    if (device_fd < 0) {
        printf(COLOR_RED "Open a device first.\n" COLOR_RESET);
        return -1;
    }

    printf("Insert the amount of data you want to read: ");
    int amount;
    int res;
    fgets(data_buff, sizeof(data_buff), stdin);

    if (atoi(data_buff) < 0) {
        printf("Insert a positive byte quantity to read\n");
        return (-1);
    }
    amount = atoi(data_buff);

    clear_buffer();
    res = read(device_fd, data_buff, min(amount, 4096));
    if (res == 0 || res == -1)
        printf(COLOR_RED "\nRead result | no data was read from the device file \n" COLOR_RESET);
    else {
        printf("\n%sRead result |  (%d bytes)%s:%s\n\n", COLOR_GREEN, res, COLOR_RESET, data_buff);
    }
}

int show_menu() {
    system("clear");
    printf(COLOR_YELLOW "      ╔═══════════════════════════════╗\n" COLOR_RESET);
    printf("      %s║%s MultiFlow Device Driver - CLI ║\n", COLOR_YELLOW, BOLD);
    printf(COLOR_YELLOW "      ╚═══════════════════════════════╝\n" COLOR_RESET);
    printf(COLOR_YELLOW "┌───────────────────────────────────────────┐\n" COLOR_RESET);
    printf("%s│%s%s Currently Opened Device:%s ", COLOR_YELLOW, COLOR_RESET, BOLD, RESET);
    if (device_fd == -1) {
        printf(COLOR_RED "%s\n" COLOR_RED, opened_device);
    } else {
        printf(COLOR_GREEN "%s\n" COLOR_GREEN, opened_device);
    }
    printf(COLOR_YELLOW "├───────────────────────────────────────────┤\n" COLOR_RESET);

    for (i = 0; i < menu_size; ++i) {
        printf(COLOR_YELLOW "│ ");
        printf("%s%s%s\n", COLOR_BLUE, main_menu_list[i], COLOR_RESET);
    }
    printf(COLOR_YELLOW "├───────────────────────────────────────────┤\n" COLOR_RESET);
    printf(COLOR_YELLOW "│" COLOR_RESET);
    printf(BOLD " > Insert your command: " RESET);
    fgets(data_buff, sizeof(data_buff), stdin);
    printf(COLOR_YELLOW "└───────────────────────────────────────────┘\n\n" COLOR_RESET);
    cmd = atoi(data_buff);
    clear_buffer();
}

int create_nodes() {
    printf("Creating %d minors for device %s with major %d\n", NUM_DEVICES, device_path, major);

    delete_nodes();
    for (i = 0; i < NUM_DEVICES; i++) {
        sprintf(data_buff, "mknod %s%d c %d %i\n", device_path, i, major, i);
        system(data_buff);
        sprintf(data_buff, "%s%d", device_path, i);
    }
}

int delete_nodes() {
    int minors;
    clear_buffer();
    sprintf(data_buff, "rm %s*\n", device_path);
    if (device_fd != -1) {
        sprintf(opened_device, "none");
        device_fd = -1;
        close(device_fd);
    }
    system(data_buff);
}

/**
 * Abilita o disabilita un device file accedendo all'apposito parametro nel VFS
 */
int set_enabling(int minor, int status) {
    char* filename = "/sys/module/test/parameters/device_enabling";
    FILE* handle = fopen(filename, "w+"); /* open the file for reading and updating */

    if (handle == NULL) return -1; /* if file not found quit */
    int count = 0;
    char current_char = 'n';
    int swap_pos = 0;
    char* line = NULL;
    size_t len = 0;

    getline(&line, &len, handle);
    printf("line: %s", line);
    char s = status + '0';
    line[2 * minor] = s;
    printf("line: %s", line);
    fputs(line, handle);
    getline(&line, &len, handle);
    printf("line: %s", line);
    // while (current_char != EOF) {
    //     current_char = fgetc(handle);
    //     printf("stream pos: %ld | ", ftell(handle));
    //     printf("current = %c | ", current_char);
    //     printf("count = %d\n", count);

    //     if (count == minor) {
    //         printf("count==minor\n");
    //         fseek(handle, 2, 0);
    //         fprintf(handle, "%d", status); /* write the new character at the new position */
    //         swap_pos = count;
    //     }

    //     if (current_char == ',') {
    //         count++;
    //     }
    // }
    fclose(handle); /* it's important to close the file_handle
                       when you're done with it to avoid memory leaks */
    return 0;
}

int exit_op() {
    // TODO aggiungere sblocco mutex rilascio risorse e quant'altro
    printf(COLOR_CYAN "Goodbye.\n" COLOR_RESET);
    exit(0);
}
