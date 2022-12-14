/*
=====================================================================================================
                                            user_cli.c
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

char* session_priority = HIGH_PRIORITY;
char* session_blocking = NON_BLOCKING;
int session_timeout = 0;

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

/**
 * Main del programma. Mostra la lista di comandi disponibili e le informazioni sul dispositivo aperto.
 * Richiede in un loop infinito l'inserimento di un comando all'utente, mostrando poi il risultato dell'operazione
 */
int main(int argc, char** argv) {
    int ret;
    if (argc != 2 && argc != 3) {
        print_menu_header();
        printf(COLOR_RED BOLD "> Launch error: wrong number of arguments\n" RESET);
        printf(COLOR_GREEN "> USAGE: sudo ./user_cli MAJOR [DEVICE_PATH].\n" RESET);
        return -1;
    }
    if (isNumber(argv[1])) {
        major = atoi(argv[1]);
    } else {
        print_menu_header();
        printf(COLOR_RED BOLD "> Launch error: insert a numeric value for MAJOR.\n" RESET);
        printf(COLOR_GREEN "> USAGE: sudo ./user_cli MAJOR [DEVICE_PATH].\n" RESET);
        return -1;
    }

    if (argv[2] == 0) {
        device_path = DEFAULT_DEV_PATH;
    } else {
        device_path = argv[2];
    }

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
                session_priority = LOW_PRIORITY;
                setup_session(cmd);
                wait_input();
                break;
            case (4):
                session_priority = HIGH_PRIORITY;
                setup_session(cmd);
                wait_input();
                break;
            case (5):
                session_blocking = BLOCKING;
                setup_session(cmd);
                wait_input();
                break;
            case (6):
                session_blocking = NON_BLOCKING;
                setup_session(cmd);
                wait_input();
                break;
            case (7):
                setup_session(cmd);
                wait_input();
                break;
            case (8):
                set_device_enabling(1);
                break;
            case (9):
                set_device_enabling(0);
                break;
            case (10):
                show_device_status();
                wait_input();
                break;
            case (11):
                create_nodes();
                wait_input();
                break;
            case (12):
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
 * Apre una sessione verso un device file, specificato dall'utente tramite input da tastiera
 */
int open_device() {
    // Richiesta minor da aprire all'utente
    printf("Insert Minor Number of the device driver to open: ");
    fgets(data_buff, sizeof(data_buff), stdin);
    if (strcmp(data_buff, "\n") == 0) {
        printf("Operation canceled.\n");
        return 0;
    }
    if (!isNumber(data_buff)) {
        printf(COLOR_RED "Insert a numeric value, between 0 and 127\n" RESET);
        return -1;
    } else if (atoi(data_buff) < 0 || atoi(data_buff) > 127) {
        printf(COLOR_RED "Insert a valid minor, between 0 and 127\n" RESET);
        return -1;
    }

    int old_minor = minor;
    minor = atoi(data_buff);
    clear_buffer();

    // Se ?? aperto un altro dispositivo lo chiudo prima di aprire quello nuovo. Ogni client pu?? mantenere un solo dispositivo aperto per volta.
    if (device_fd != -1) {
        if (old_minor == minor) {
            printf("Device %s is already opened.\n", opened_device);
            return 0;
        }
        printf("There is an opened device %s. Closing it.\n", opened_device);
        close(device_fd);
        sprintf(opened_device, "none");
        device_fd = -1;
    }

    // Apertura del file specificato
    sprintf(opened_device, "%s%d", device_path, minor);
    device_fd = open(opened_device, O_RDWR);
    if (device_fd == -1) {
        if (read_param_field(DEVICE_ENABLING_PATH, minor) == 0) {
            printf("%sThe device %s is actually disabled. Enable it before opening %s\n", COLOR_RED, opened_device, RESET);
        } else if (errno == EPERM) {
            printf(COLOR_RED "Open error, invalid permissions on device %s.\n" RESET, opened_device);
        } else if (errno == ENOENT) {
            printf(COLOR_RED "Open error, device %s does not exists.\n" RESET, opened_device);
        }
        sprintf(opened_device, "none");
        device_fd = -1;
        return -1;
    }
    printf("%sDevice %s successfully opened, fd is: %d%s\n", COLOR_GREEN, opened_device, device_fd, RESET);

    // Controllo se il file aperto ha un major differente da quello del client
    opened_major = get_open_major(opened_device);
    if (opened_major != major) {
        printf("%s%sWarning: currently opened device has major '%d' different than '%d' used by the CLI.%s\n", COLOR_YELLOW, BOLD, opened_major, major, RESET);
        printf("%sChange the Major used by the client to '%d' or recreate the nodes with Major '%d'.%s\n", COLOR_YELLOW, opened_major, major, RESET);
    }

    return 0;
}

/**
 * Scrive i dati inseriti da stdin nel device attualmente aperto.
 */
int write_op() {
    int res;

    if (device_fd < 0) {
        printf(COLOR_RED "No device actually opened. Open a device first.\n" RESET);
        return -1;
    }

    clear_buffer();

    printf("Insert the data you want to write (max 4096): ");
    fgets(data_buff, sizeof(data_buff), stdin);
    if (strcmp(data_buff, "\n") == 0) {
        printf("Operation canceled.\n");
        return 0;
    }

    data_buff[strcspn(data_buff, "\n")] = 0;  // ignoring \n

    res = write(device_fd, data_buff, min(strlen(data_buff), 4096));
    if (res < 0) {
        printf(COLOR_RED "\nWrite failed, check 'dmesg' for more info.\n" RESET);
    } else {
        printf("\n%sWrite success, %d bytes have been written on the device.%s\n", COLOR_GREEN, res, RESET);
    }
    return res;
}

/**
 * Legge la quantit?? di byte desiderata dal device attualmente aperto
 */
int read_op() {
    int amount;
    int res;

    if (device_fd < 0) {
        printf(COLOR_RED "No device actually opened. Open a device first.\n" RESET);
        return NO_DEV;
    }

    printf("Insert the amount of data you want to read: ");
    fgets(data_buff, sizeof(data_buff), stdin);

    if (strcmp(data_buff, "\n") == 0) {
        printf("Operation canceled.\n");
        return 0;
    }
    if (!isNumber(data_buff)) {
        printf(COLOR_RED "Insert a numeric value for bytes to read.\n" RESET);
        return -1;
    } else if (atoi(data_buff) < 0) {
        printf(COLOR_RED "Insert a positive byte quantity to read.\n" RESET);
        return -1;
    }

    amount = atoi(data_buff);

    clear_buffer();
    res = read(device_fd, data_buff, min(amount, 4096));
    if (res < 0) {
        printf(COLOR_RED "\nRead failed, check 'dmesg' for more info.\n" RESET);
    } else {
        printf("\n%sRead success, %d bytes have been read from the device%s: %s\n\n", COLOR_GREEN, res, RESET, data_buff);
    }

    return res;
}

/**
 * Mostra il menu con i possibili comandi utente. Se c'?? un dispositivo aperto vengono mostrate anche informazioni sullo stato attuale.
 */
int show_menu() {
    system("clear");
    print_menu_header();
    printf(COLOR_YELLOW "???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????\n" RESET);
    printf("%s???%s%s Currently Opened Device:%s ", COLOR_YELLOW, RESET, BOLD, RESET);
    if (device_fd == -1) {
        printf(COLOR_RED "%s\n" RESET, opened_device);
    } else {
        printf(COLOR_GREEN "%s\n" RESET, opened_device);
        long available_space = MAX_SIZE_BYTES - read_param_field(TOTAL_BYTES_LOW_PATH, minor) - read_param_field(TOTAL_BYTES_HIGH_PATH, minor);
        printf("%s???%s%s Estimated Available Space:%s %ld bytes\n", COLOR_YELLOW, RESET, BOLD, RESET, available_space);
        printf(COLOR_YELLOW "???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????\n" RESET);
        printf("%s??? Session Priority:%s %s\n", COLOR_YELLOW, RESET, session_priority);
        printf("%s??? Session Blocking Type:%s %s\n", COLOR_YELLOW, RESET, session_blocking);
        if (strcmp(session_priority, LOW_PRIORITY) == 0) {
            printf("%s??? Total Bytes to Read:%s %d\n", COLOR_YELLOW, RESET, read_param_field(TOTAL_BYTES_LOW_PATH, minor));
            printf("%s??? Number of Waiting Threads:%s %d\n", COLOR_YELLOW, RESET, read_param_field(WAITING_THREADS_LOW_PATH, minor));
        } else {
            printf("%s??? Total Bytes to Read:%s %d\n", COLOR_YELLOW, RESET, read_param_field(TOTAL_BYTES_HIGH_PATH, minor));
            printf("%s??? Number of Waiting Threads:%s %d\n", COLOR_YELLOW, RESET, read_param_field(WAITING_THREADS_HIGH_PATH, minor));
        }
        if (strcmp(session_blocking, BLOCKING) == 0) {
            printf("%s??? Blocking Timeout:%s %d ms\n", COLOR_YELLOW, RESET, session_timeout);
        }
    }
    printf(COLOR_YELLOW "???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????\n" RESET);

    for (i = 0; i < menu_size; ++i) {
        printf(COLOR_YELLOW "??? ");
        printf("%s%s%s\n", COLOR_BLUE, main_menu_list[i], RESET);
    }
    printf(COLOR_YELLOW "???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????\n" RESET);
    printf(COLOR_YELLOW "???" RESET);
    printf(BOLD " > Insert your command: " RESET);
    fgets(data_buff, sizeof(data_buff), stdin);
    printf(COLOR_YELLOW "???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????\n\n" RESET);

    if (strcmp(data_buff, "\n") == 0) {
        cmd = 12;
    } else if (isNumber(data_buff)) {
        cmd = atoi(data_buff);
    } else {
        cmd = 13;
    }
    clear_buffer();
}

/**
 * Crea 128 nodi con minor 0-127 in /dev
 */
int create_nodes() {
    printf("Creating %d minors for device %s with major %d\n", NUM_DEVICES, device_path, major);
    char the_dev[128];
    int n = 0;
    for (i = 0; i < NUM_DEVICES; i++) {
        sprintf(the_dev, "%s%d", device_path, i);
        if (access(the_dev, F_OK) != 0) {
            sprintf(data_buff, "mknod %s c %d %i\n", the_dev, major, i);
            system(data_buff);
            sprintf(data_buff, "%s%d", device_path, i);
            n++;
        } else {
            // Il nodo non viene creato se gi?? esiste.
            // printf("Device %s already exists. Skipping creation.\n", the_dev);
        }
    }
    printf(COLOR_GREEN "Succesfully created %d device files.\n" RESET, n);
    return 0;
}

/**
 * Abilita o disabilita un device file accedendo all'apposito parametro esposto nel VFS
 */
int set_device_enabling(int status) {
    int count = 0;
    char current_char = 'n';
    int swap_pos = 0;
    char* line = NULL;
    size_t len = 0;
    int minor_cmd;

    // Apertura del file relativo al module_param_array 'device_enabling'
    FILE* handle = fopen(DEVICE_ENABLING_PATH, "w+");
    if (handle == NULL) {
        return -1;
    }

    char* op = "enable";

    if (status == 0) {
        op = "disable";
    }

    // Richiesta all'utente del minor che si vuole abilitare/disabilitare
    printf("Enter the minor number of the device to %s: ", op);
    fgets(data_buff, sizeof(data_buff), stdin);
    if (strcmp(data_buff, "\n") == 0) {
        printf("Operation canceled.\n");
        return 0;
    }
    if (!isNumber(data_buff)) {
        printf(COLOR_RED "Insert a numeric value, between 0 and %d\n" RESET, NUM_DEVICES - 1);
        wait_input();
        return -1;
    } else if (atoi(data_buff) < 0 || atoi(data_buff) > NUM_DEVICES - 1) {
        printf(COLOR_RED "Insert a valid minor, between 0 and %d\n" RESET, NUM_DEVICES - 1);
        wait_input();
        return -1;
    }
    minor_cmd = atoi(data_buff);
    clear_buffer();

    // Scrittura del valore 1/0 nel file device_enabling, alla posizione associata al minor
    getline(&line, &len, handle);
    char s = status + '0';
    line[2 * minor_cmd] = s;
    fputs(line, handle);
    fclose(handle);
    printf(COLOR_GREEN "Device file succesfully %sd.\n" RESET, op);

    wait_input();
    return 0;
}

/**
 * Mostra lo stato attuale di un device file accedendo a tutti i parametri esposti nel VFS
 */
int show_device_status() {
    int minor_cmd;

    printf("Enter the minor number of the device: ");
    fgets(data_buff, sizeof(data_buff), stdin);
    if (strcmp(data_buff, "\n") == 0) {
        printf("Operation canceled.\n");
        return 0;
    }
    if (!isNumber(data_buff)) {
        printf(COLOR_RED "Insert a numeric value, between 0 and %d\n" RESET, NUM_DEVICES - 1);
        return -1;
    } else if (atoi(data_buff) < 0 || atoi(data_buff) > NUM_DEVICES - 1) {
        printf(COLOR_RED "Insert a valid minor, between 0 and %d\n" RESET, NUM_DEVICES - 1);
        return -1;
    }
    minor_cmd = atoi(data_buff);
    clear_buffer();

    printf("???????????????????????????????????????????????????????????????????????????????????????????????????????????????\n");
    printf("???%s   Device /dev/test-dev%d Status %s\n", BOLD, minor_cmd, RESET);
    printf("???????????????????????????????????????????????????????????????????????????????????????????????????????????????\n");

    char* op = "ENABLED";
    if (read_param_field(DEVICE_ENABLING_PATH, minor_cmd) == 0) {
        op = "DISABLED";
    }
    long available_space = MAX_SIZE_BYTES - read_param_field(TOTAL_BYTES_LOW_PATH, minor) - read_param_field(TOTAL_BYTES_HIGH_PATH, minor);

    printf("??? %sDevice Status :%s %s\n", BOLD, RESET, op);
    printf("??? %sAvailable Space:%s %ld bytes\n", BOLD, RESET, available_space);
    printf("??? %sHigh Priority Bytes:%s %d\n", BOLD, RESET, read_param_field(TOTAL_BYTES_HIGH_PATH, minor_cmd));
    printf("??? %sLow Priority Bytes:%s %d\n", BOLD, RESET, read_param_field(TOTAL_BYTES_LOW_PATH, minor_cmd));
    printf("??? %sHigh Priority Waiting Threads:%s %d\n", BOLD, RESET, read_param_field(WAITING_THREADS_HIGH_PATH, minor_cmd));
    printf("??? %sLow Priority Waiting Threads:%s %d\n", BOLD, RESET, read_param_field(WAITING_THREADS_LOW_PATH, minor_cmd));
    printf("??? %sTimeout value:%s %d\n", BOLD, RESET, session_timeout);
    printf("???????????????????????????????????????????????????????????????????????????????????????????????????????????????\n");
}

/**
 * Modifica i parametri della sessione tramite ioctl()
 */
int setup_session(int op) {
    int timeout;
    int ret;

    // Verifica se c'?? una sessione attiva
    if (device_fd == -1) {
        printf(COLOR_RED "No active session. Open a device first.\n" RESET);
        return -1;
    }

    // Solo se l'operazione ?? di set_timeout si richiede all'utente il valore da impostare
    if (op != 7) {
        timeout = 0;
    } else {
        printf("Insert timeout in milliseconds: ");
        fgets(data_buff, sizeof(data_buff), stdin);

        if (strcmp(data_buff, "\n") == 0) {
            printf("Operation canceled.\n");
            return 0;
        }
        if (!isNumber(data_buff)) {
            printf(COLOR_RED "Insert a numeric value for lock timeout.\n" RESET);
            return -1;
        } else if (atoi(data_buff) < 0) {
            printf(COLOR_RED "Insert a positive value for timeout (>0ms).\n" RESET);
            return -1;
        }
        timeout = atoi(data_buff);
        session_timeout = timeout;
    }
    // Esecuzione di ioctl
    ret = ioctl(device_fd, op, timeout);
    if (ret == -1) {
        printf(COLOR_RED "Error executing ioctl\n" RESET);
        return -1;
    }
    printf(COLOR_GREEN "Session settings correctly changed\n" RESET);
    return 0;
}

/**
 * Termina il client, chiudendo il file se c'?? una sessione attiva verso un dispositivo
 */
int exit_op() {
    if (device_fd != -1) {
        close(device_fd);
    }
    printf(COLOR_CYAN "Goodbye.\n" RESET);
    exit(0);
}
