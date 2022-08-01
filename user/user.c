#include "utils.h"

int i;
char data_buff[4096];
int cmd;
int device_fd = -1;
int minor;
int major;
char* device_path;
char opened_device[64] = "none";
int opened_major = 0;

/**
 * Lista di comandi utilizzabili
 */
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
                printf(COLOR_RED "Insert a valid command from the list\n" COLOR_RESET);
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

    opened_major = get_open_major(opened_device);

    if (opened_major != major) {
        printf("%s%s\nWarning: currently open device has a different major than the one used by CLI.%s\n", COLOR_YELLOW, BOLD, COLOR_RESET);
        printf("%sTry using command (11) to re-create the device using the current driver.%s\n", COLOR_YELLOW, COLOR_RESET);
    }

    return 0;
}

/**
 * Scrive i dati specificati da stdin nel device attualmente aperto
 */
int write_op() {
    if (device_fd < 0) {
        printf(COLOR_RED "No device actually opened. Open a device first..\n" COLOR_RESET);
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

/**
 * Legge la quantità di byte desiderata dal device attualmente aperto
 */
int read_op() {
    if (device_fd < 0) {
        printf(COLOR_RED "No device actually opened. Open a device first..\n" COLOR_RESET);
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

/**
 * Mostra il menu con i possibili comandi utente
 */
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
            printf(COLOR_RED "No device actually opened. Open a device first..\n" COLOR_RESET);
            return -1;
        }
        minor_cmd = minor;
    }
    if (minor_cmd < -1 || minor_cmd > NUM_DEVICES - 1) {
        printf(COLOR_RED "Insert a valid minor 0-127\n" COLOR_RESET);
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

    printf("Enter the minor number of the device (-1 for currently opened device): ");
    fgets(data_buff, sizeof(data_buff), stdin);
    minor_cmd = atoi(data_buff);
    clear_buffer();
    if (minor_cmd == -1) {
        if (device_fd == -1) {
            printf(COLOR_RED "No device actually opened. Open a device first..\n" COLOR_RESET);
            return -1;
        }
        minor_cmd = minor;
    }
    if (minor_cmd < -1 || minor_cmd > NUM_DEVICES - 1) {
        printf(COLOR_RED "Insert a valid minor 0-127\n" COLOR_RESET);
        return -1;
    }

    printf("┌───────────────────────────────────┐\n");
    printf("│%s   Device /dev/test-dev%d Status %s\n", BOLD, minor_cmd, RESET);
    printf("├───────────────────────────────────┤\n");
    char* enabling = "/sys/module/test/parameters/device_enabling";
    char* high_b = "/sys/module/test/parameters/total_bytes_high";
    char* low_b = "/sys/module/test/parameters/total_bytes_low";
    char* high_t = "/sys/module/test/parameters/waiting_threads_high";
    char* low_t = "/sys/module/test/parameters/waiting_threads_low";

    char* op = "ENABLED";
    if (read_param_field(enabling, minor_cmd) == 0) {
        op = "DISABLED";
    }

    printf("│ %sOperativity:%s %s\n", BOLD, RESET, op);
    printf("│ %sHigh Priority Bytes:%s %d\n", BOLD, RESET, read_param_field(high_b, minor_cmd));
    printf("│ %sLow Priority Bytes:%s %d\n", BOLD, RESET, read_param_field(low_b, minor_cmd));
    printf("│ %sLow Priority Waiting Threads:%s %d\n", BOLD, RESET, read_param_field(high_t, minor_cmd));
    printf("│ %sHigh Priority Waiting Threads:%s %d\n", BOLD, RESET, read_param_field(low_t, minor_cmd));

    printf("└───────────────────────────────────┘\n");
}

/**
 * Chiude il programma liberando tutte le risorse
 */
int exit_op() {
    // TODO aggiungere sblocco mutex rilascio risorse e quant'altro
    printf(COLOR_CYAN "Goodbye.\n" COLOR_RESET);
    exit(0);
}
