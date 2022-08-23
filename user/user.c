/*
=====================================================================================================
                                            user.c
-----------------------------------------------------------------------------------------------------
    Implementa una semplice Command Line Interface per interagire con i dispositivi tramite il
                                        multiflow-driver
=====================================================================================================
*/

#include "utils.h"

int i;
int cmd;
char data_buff[4096];
int minor;
int major;
char* device_path;
char opened_device[64] = "none";
int opened_major = 0;
int device_fd = -1;

int menu_size = sizeof(main_menu_list) / sizeof(char*);

/**
 * Prototipi
 */
int show_menu();
int create_nodes();
int delete_nodes();
int write_op();
int read_op();
int open_device();
int exit_op();
int set_device_enabling(int status);
int show_device_status();
int setup_session(int cmd);

/**
 * Resetta a 0 i bytes del buffer utilizzato nelle varie operazioni
 */
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
            case (4):
            case (5):
            case (6):
            case (7):
                setup_session(cmd);
                wait_input();
                break;
            case (8):
                set_device_enabling(1);
                wait_input();
                break;
            case (9):
                set_device_enabling(0);
                wait_input();
                break;
            case (10):
                show_device_status();
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
                printf(COLOR_RED "Insert a valid command from the list\n" RESET);
                wait_input();
                break;
        }
    }
    return 0;
}

/**
 * Apre una sessione verso un device file
 */
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
        printf("Device %s is already opened.\n", opened_device);
        return 0;
    }

    sprintf(opened_device, "%s%d", device_path, minor);
    printf("Opening device %s\n", opened_device);
    device_fd = open(opened_device, O_RDWR);
    if (device_fd == -1) {
        if (read_param_field(DEVICE_ENABLING_PATH, minor) == 0) {
            printf("%sThe device %s is actually disabled. Enable it before opening %s\n", COLOR_RED, opened_device, RESET);
            sprintf(opened_device, "none");
            return -1;
        }
        printf(COLOR_RED "Open error on device %s, use 'dmesg' for details.\n", opened_device);
        sprintf(opened_device, "none");
        device_fd = -1;
        return -1;
    }

    printf("%sDevice %s successfully opened, fd is: %d%s\n", COLOR_GREEN, opened_device, device_fd, RESET);

    opened_major = get_open_major(opened_device);

    if (opened_major != major) {
        printf("%s%s\nWarning: currently open device has major '%d' different than '%d' used by the CLI.%s\n", COLOR_YELLOW, BOLD, opened_major, major, RESET);
        printf("%sTry using command (11) to re-create the device node using the current driver.%s\n", COLOR_YELLOW, RESET);
    }

    return 0;
}

/**
 * Scrive i dati specificati da stdin nel device attualmente aperto
 */
int write_op() {
    if (device_fd < 0) {
        printf(COLOR_RED "No device actually opened. Open a device first.\n" RESET);
        return -1;
    }

    int res;
    clear_buffer();
    printf("Insert the data you want to write (max 4096): ");
    fgets(data_buff, sizeof(data_buff), stdin);
    data_buff[strcspn(data_buff, "\n")] = 0;  // ignoring \n

    res = write(device_fd, data_buff, min(strlen(data_buff), 4096));
    if (res <= 0) {
        printf(COLOR_RED "\nWrite result: could not write in the buffer \n" RESET);
    } else {
        printf("\n%sWrite result (%d bytes): operation completed successfully%s\n", COLOR_GREEN, res, RESET);
    }
}

/**
 * Legge la quantità di byte desiderata dal device attualmente aperto
 */
int read_op() {
    if (device_fd < 0) {
        printf(COLOR_RED "No device actually opened. Open a device first..\n" RESET);
        return -1;
    }

    printf("Insert the amount of data you want to read: ");
    int amount;
    int res;
    fgets(data_buff, sizeof(data_buff), stdin);

    if (atoi(data_buff) < 0) {
        printf("Insert a positive byte quantity to read\n");
        return -1;
    }
    amount = atoi(data_buff);

    clear_buffer();
    res = read(device_fd, data_buff, min(amount, 4096));
    if (res == 0 || res == -1)
        printf(COLOR_RED "\nRead result | no data was read from the device file \n" RESET);
    else {
        printf("\n%sRead result (%d bytes)%s: %s\n\n", COLOR_GREEN, res, RESET, data_buff);
    }

    return res;
}

/**
 * Mostra il menu con i possibili comandi utente
 */
int show_menu() {
    system("clear");
    printf(COLOR_YELLOW "      ╔═══════════════════════════════╗\n" RESET);
    printf("      %s║%s MultiFlow Device Driver - CLI ║\n", COLOR_YELLOW, BOLD);
    printf(COLOR_YELLOW "      ╚═══════════════════════════════╝\n" RESET);
    printf(COLOR_YELLOW "┌───────────────────────────────────────────┐\n" RESET);
    printf("%s│%s%s Currently Opened Device:%s ", COLOR_YELLOW, RESET, BOLD, RESET);
    if (device_fd == -1) {
        printf(COLOR_RED "%s\n" COLOR_RED, opened_device);
    } else {
        printf(COLOR_GREEN "%s\n" RESET, opened_device);
        printf(COLOR_YELLOW "├───────────────────────────────────────────┤\n" RESET);
        printf("│ %sHigh Priority Bytes:%s %d\n", BOLD, RESET, read_param_field(TOTAL_BYTES_HIGH_PATH, minor));
        printf("│ %sLow Priority Bytes:%s %d\n", BOLD, RESET, read_param_field(TOTAL_BYTES_LOW_PATH, minor));
        printf("│ %sHigh Priority Waiting Threads:%s %d\n", BOLD, RESET, read_param_field(WAITING_THREADS_HIGH_PATH, minor));
        printf("│ %sLow Priority Waiting Threads:%s %d\n", BOLD, RESET, read_param_field(WAITING_THREADS_LOW_PATH, minor));
    }
    printf(COLOR_YELLOW "├───────────────────────────────────────────┤\n" RESET);

    for (i = 0; i < menu_size; ++i) {
        printf(COLOR_YELLOW "│ ");
        printf("%s%s%s\n", COLOR_BLUE, main_menu_list[i], RESET);
    }
    printf(COLOR_YELLOW "├───────────────────────────────────────────┤\n" RESET);
    printf(COLOR_YELLOW "│" RESET);
    printf(BOLD " > Insert your command: " RESET);
    fgets(data_buff, sizeof(data_buff), stdin);
    printf(COLOR_YELLOW "└───────────────────────────────────────────┘\n\n" RESET);
    cmd = atoi(data_buff);
    clear_buffer();
}

/**
 * Crea 128 nodi con minor 0-127 in /dev
 */
int create_nodes() {
    printf("Creating %d minors for device %s with major %d\n", NUM_DEVICES, device_path, major);

    delete_nodes();
    for (i = 0; i < NUM_DEVICES; i++) {
        sprintf(data_buff, "mknod %s%d c %d %i\n", device_path, i, major, i);
        system(data_buff);
        sprintf(data_buff, "%s%d", device_path, i);
    }
    printf(COLOR_GREEN "Device files succesfully created.\n" RESET);
    return 0;
}

/**
 * Elimina tutti i nodi test-dev attualmente presenti in /dev
 */
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
int set_device_enabling(int status) {
    char* filename = "/sys/module/test/parameters/device_enabling";
    int count = 0;
    char current_char = 'n';
    int swap_pos = 0;
    char* line = NULL;
    size_t len = 0;
    int minor_cmd;

    // Open del file relativo al module_param_array 'device_enabling'
    FILE* handle = fopen(filename, "w+");
    if (handle == NULL) {
        return -1;
    }

    char* op = "enable";

    if (status == 0) {
        op = "disable";
    }

    printf("Enter the minor number of the device to %s (-1 for currently opened device): ", op);
    fgets(data_buff, sizeof(data_buff), stdin);
    minor_cmd = atoi(data_buff);
    if (minor_cmd == -1) {
        if (device_fd == -1) {
            printf(COLOR_RED "No device actually opened. Open a device first..\n" RESET);
            return -1;
        }
        minor_cmd = minor;
    }
    if (minor_cmd < -1 || minor_cmd > NUM_DEVICES - 1) {
        printf(COLOR_RED "Insert a valid minor 0-127\n" RESET);
        return -1;
    }
    clear_buffer();

    getline(&line, &len, handle);
    char s = status + '0';
    line[2 * minor_cmd] = s;
    fputs(line, handle);
    fclose(handle);
    return 0;
}

/**
 * Mostra lo stato attuale di un device file accedendo a tutti i parametri esposti nel VFS
 */
int show_device_status() {
    int minor_cmd;

    printf("Enter the minor number of the device: ");
    fgets(data_buff, sizeof(data_buff), stdin);
    minor_cmd = atoi(data_buff);
    clear_buffer();
    if (minor_cmd < 0 || minor_cmd > NUM_DEVICES - 1) {
        printf(COLOR_RED "Insert a valid minor 0-127\n" RESET);
        return -1;
    }

    printf("┌───────────────────────────────────┐\n");
    printf("│%s   Device /dev/test-dev%d Status %s\n", BOLD, minor_cmd, RESET);
    printf("├───────────────────────────────────┤\n");

    char* op = "ENABLED";
    if (read_param_field(DEVICE_ENABLING_PATH, minor_cmd) == 0) {
        op = "DISABLED";
    }

    printf("│ %sOperativity:%s %s\n", BOLD, RESET, op);
    printf("│ %sHigh Priority Bytes:%s %d\n", BOLD, RESET, read_param_field(TOTAL_BYTES_HIGH_PATH, minor_cmd));
    printf("│ %sLow Priority Bytes:%s %d\n", BOLD, RESET, read_param_field(TOTAL_BYTES_LOW_PATH, minor_cmd));
    printf("│ %sHigh Priority Waiting Threads:%s %d\n", BOLD, RESET, read_param_field(WAITING_THREADS_HIGH_PATH, minor_cmd));
    printf("│ %sLow Priority Waiting Threads:%s %d\n", BOLD, RESET, read_param_field(WAITING_THREADS_LOW_PATH, minor_cmd));
    printf("└───────────────────────────────────┘\n");
}

/**
 * Modifica i parametri della sessione tramite ioctl()
 */
int setup_session(int op) {
    int timeout;
    int ret;

    if (device_fd == -1) {
        printf(COLOR_RED "No active session. Open a device first.\n" RESET);
        return -1;
    }
    if (op != 7) {
        timeout = 0;
    } else {
        printf("Insert timeout in milliseconds: ");
        fgets(data_buff, sizeof(data_buff), stdin);

        if (atoi(data_buff) < 1) {
            printf(COLOR_RED "Insert a positive (>0ms) timeout value\n" RESET);
            return -1;
        }
    }
    timeout = atoi(data_buff);
    ret = ioctl(device_fd, op, timeout);
    if (ret == -1) {
        printf(COLOR_RED "Error executing ioctl\n" RESET);
        return -1;
    }
    printf(COLOR_GREEN "Session settings correctly changed\n" RESET);
}

/**
 * Chiude il programma liberando tutte le risorse
 */
int exit_op() {
    // TODO aggiungere sblocco mutex rilascio risorse e quant'altro
    printf(COLOR_CYAN "Goodbye.\n" RESET);
    exit(0);
}