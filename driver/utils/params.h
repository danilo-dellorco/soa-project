/*
=====================================================================================================
                                                params.h
-----------------------------------------------------------------------------------------------------
Mantiene tutti i parametri e le costanti utilizzate all'interno del device driver
=====================================================================================================
*/

#ifndef PARAMS_H
#define PARAMS_H
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pid.h> /* For pid types */
#include <linux/sched.h>
#include <linux/tty.h>     /* For the tty declarations */
#include <linux/version.h> /* For LINUX_VERSION_CODE */
#include <linux/workqueue.h>

// #define TEST

#define MODNAME "MULTI-FLOW DEV"
#define DEVICE_NAME "mflow-dev"

#ifdef TEST
#define MAX_SIZE_BYTES 128  // Utilizzato per debugging e testing
#else
#define MAX_SIZE_BYTES 1048576  // Massima dimensione di byte mantenibili da un singolo device (1MB)
#endif

#define NUM_DEVICES 128
#define NUM_FLOWS 2

#define LOW_PRIORITY 0
#define HIGH_PRIORITY 1

#define BLOCKING 0
#define NON_BLOCKING 1

#define DISABLED 0
#define ENABLED 1

#define TEST_TIME 15000  // Tempo di attesa prima di rilasciare il lock nella fase di testing

// Codici delle operazioni dev_ioctl
#define SET_LOW_PRIORITY 3
#define SET_HIGH_PRIORITY 4
#define SET_BLOCKING_OP 5
#define SET_NON_BLOCKING_OP 6
#define SET_TIMEOUT 7
#define ENABLE_DEV 8   // Attualmente non utilizzato
#define DISABLE_DEV 9  // Attualmente non utilizzato

// Codici di ritorno
#define OPEN_ERROR -1
#define WRITE_ERROR -1
#define READ_ERROR -1
#define LOCK_NOT_ACQUIRED -1
#define LOCK_ACQUIRED 0
#define SCHED_ERROR -1

// Modalità di locking in get_lock
#define TRYLOCK 1
#define LOCK 2

/**
 *  Parametri del modulo
 */
unsigned long device_enabling[NUM_DEVICES];
module_param_array(device_enabling, ulong, NULL, 0660);
MODULE_PARM_DESC(device_enabling, "Specify if a device file is enabled or disabled. If it is disabled, any attempt to open a session will fail");

unsigned long total_bytes_low[NUM_DEVICES];
module_param_array(total_bytes_low, ulong, NULL, 0440);
MODULE_PARM_DESC(total_bytes_low, "Number of bytes yet to be read in the low priority flow.");

unsigned long total_bytes_high[NUM_DEVICES];
module_param_array(total_bytes_high, ulong, NULL, 0440);
MODULE_PARM_DESC(total_bytes_high, "Number of bytes yet to be read in the high priority flow.");

unsigned long waiting_threads_low[NUM_DEVICES];
module_param_array(waiting_threads_low, ulong, NULL, 0440);
MODULE_PARM_DESC(waiting_threads_low, "Number of threads waiting for data on the low priority flow.");

unsigned long waiting_threads_high[NUM_DEVICES];
module_param_array(waiting_threads_high, ulong, NULL, 0440);
MODULE_PARM_DESC(waiting_threads_high, "Number of threads waiting for data on the high priority flow.");

// Ritorna la stringa associata ad un codice di priorità
char* get_prio_str(int code) {
    if (code == 0) {
        return "LOW_PRIORITY";
    }
    return "HIGH_PRIORITY";
}

// Ritorna la stringa associata ad un codice per operazioni bloccanti o non-bloccanti
char* get_block_str(int code) {
    if (code == 0) {
        return "BLOCKING";
    }
    return "NON-BLOCKING";
}
#endif