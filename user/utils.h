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

#define DEVICE_ENABLING_PATH "/sys/module/test/parameters/device_enabling"
#define TOTAL_BYTES_HIGH_PATH "/sys/module/test/parameters/total_bytes_high"
#define TOTAL_BYTES_LOW_PATH "/sys/module/test/parameters/total_bytes_low"
#define WAITING_THREADS_HIGH_PATH "/sys/module/test/parameters/waiting_threads_high"
#define WAITING_THREADS_LOW_PATH "/sys/module/test/parameters/waiting_threads_low"

/**
 * Lista di comandi utilizzabili
 */
char* main_menu_list[] = {
    "\x1B[1m--------- OPERATIONS ---------\x1B[0m",
    "\x1B[1m0)\x1B[0m  Open a device file",
    "\x1B[1m1)\x1B[0m  Write on the device file",
    "\x1B[1m2)\x1B[0m  Read from the device file",
    "",
    "\x1B[1m------ SESSION SETTINGS ------\x1B[0m",
    "\x1B[1m3)\x1B[0m  Switch to LOW priority",
    "\x1B[1m4)\x1B[0m  Switch to HIGH priority",
    "\x1B[1m5)\x1B[0m  Use BLOCKING operations",
    "\x1B[1m6)\x1B[0m  Use NON-BLOCKING operations",
    "\x1B[1m7)\x1B[0m  Set timeout",
    "",
    "\x1B[1m------ DEVICE MANAGEMENT ------\x1B[0m",
    "\x1B[1m8)\x1B[0m  Enable a device file",
    "\x1B[1m9)\x1B[0m  Disable a device file",
    "\x1B[1m10)\x1B[0m See device status",
    "\x1B[1m11)\x1B[0m Create device nodes",
    "",
    "\x1B[1m-------------------------------\x1B[0m",
    "-1) Exit",
};

/**
 *  Codici di formattazione su terminale
 */
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN "\x1b[36m"

#define RESET "\x1B[0m"
#define BOLD "\x1B[1m"

/**
 * ------------------------------------------------------------
 * Funzioni ausiliari
 * ------------------------------------------------------------
 */

/**
 * Ritorna il minimo tra due interi
 */
int min(int num1, int num2) {
    if (num1 < num2)
        return num1;
    return num2;
}

/**
 * Effettua il flush dello standard input
 */
void myflush(FILE* in) {
    int ch;
    do
        ch = fgetc(in);
    while (ch != EOF && ch != '\n');
    clearerr(in);
}

/**
 * Richiede inserimento di \n prima di continuare con il flusso di esecuzione
 */
void wait_input(void) {
    // myflush(stdin);
    printf("───────────────────────────────────────────────────────────\n");
    printf("Press %s[Enter]%s to continue...", BOLD, RESET);
    fflush(stdout);
    while (getchar() != '\n') {
        // nothing
    }
}

/**
 * Funzione ausiliaria di read_param_field
 */
const char* getfield(char* line, int num) {
    const char* tok;
    for (tok = strtok(line, ",");
         tok && *tok;
         tok = strtok(NULL, ",\n")) {
        if (!--num)
            return tok;
    }
    return NULL;
}

/**
 * Legge un campo indicizzato da 'minor' all'interno di un file csv. Utilizzato per leggere il parametro
 * di un device (identificato tramite minor) dall'apposito file di parametri esposto nel VFS
 */
int read_param_field(char* file, int minor) {
    int ret;
    FILE* stream = fopen(file, "r");

    char line[1024];
    fgets(line, 1024, stream);
    char* tmp = strdup(line);
    ret = atoi(getfield(tmp, minor + 1));
    free(tmp);
    return ret;
}

/**
 * Ottiene il major number del file attualmente aperto
 */
int get_open_major(char* opened_device) {
    char buff[4096];
    char* line;
    size_t len = 0;

    sprintf(buff,
            "ls -l %s | cut -d ' ' -f 5 | sed 's/.$//' > opened_major",
            opened_device);

    system(buff);

    FILE* ptr;
    char ch;
    ptr = fopen("opened_major", "r");

    if (NULL == ptr) {
        printf("file can't be opened \n");
    }

    getline(&line, &len, ptr);
    fclose(ptr);
    system("rm opened_major 1>/dev/null");

    return atoi(line);
}