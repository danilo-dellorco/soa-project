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

#define NUM_DEVICES 3  // TODO set to 128
#define NUM_FLOWS 2    // TODO set to 128

#define LOW_PRIORITY 0
#define HIGH_PRIORITY 1

#define BLOCKING 0
#define NON_BLOCKING 1

#define DISABLED 0
#define ENABLED 1

#define MODNAME "MULTI-FLOW DEV"
#define DEVICE_NAME "test-dev" /* Device file name in /dev/ - not mandatory  */

#define TEST_TIME 30000  // Tempo di attesa prima di rilasciare il lock nella fase di testing

/**
 *  Parametri del modulo
 */

// Posso scrivere sullo pseudofile per abilitare o disabilitare uno specifico device
unsigned long device_enabling[NUM_DEVICES];
module_param_array(device_enabling, ulong, NULL, 0660);
MODULE_PARM_DESC(device_enabling, "Specify if a device file is enabled or disabled. If it is disabled, any attempt to open a session will fail");

// Non posso modificare manualmente il numero dei bytes o il numero di thread in attesa, in quanto sono informazioni controllate indirettamente dal modulo.
// Posso comunque leggere queste informazioni
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

char *get_prio_str(int code) {
    if (code == 0) {
        return "LOW_PRIORITY";
    }
    return "HIGH_PRIORITY";
}

char *get_block_str(int code) {
    if (code == 0) {
        return "BLOCKING";
    }
    return "NON-BLOCKING";
}
#endif