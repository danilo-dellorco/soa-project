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

/**
 *  Codici di formattazione su terminale
 */
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_RESET "\x1b[0m"

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
    getchar();
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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