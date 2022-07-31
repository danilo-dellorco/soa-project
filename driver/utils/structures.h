#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pid.h> /* For pid types */
#include <linux/sched.h>
#include <linux/tty.h>     /* For the tty declarations */
#include <linux/version.h> /* For LINUX_VERSION_CODE */

#define NUM_DEVICES 3  // TODO set to 128

#define LOW_PRIORITY 0
#define HIGH_PRIORITY 1

#define BLOCKING 0
#define NON_BLOCKING 1

#define DISABLED 0
#define ENABLED 1

#define MODNAME "MULTI-FLOW DEV"
#define DEVICE_NAME "test-dev" /* Device file name in /dev/ - not mandatory  */

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

/**
 * Singolo blocco dati relativo allo stream
 */
typedef struct _stream_block {
    int read_offset;             // Mantiene l'offset di lettura del blocco corrente
    char *stream_content;        // Il nodo di I/O è un buffer di memoria, che viene puntato tramite questo campo
    struct _stream_block *next;  // Puntatore al blocco di stream successivo
    int id;                      // ID progressivo del blocco, utile per debugging
} stream_block;

/**
 * Mantinene lo stato del device
 */
typedef struct _object_state {
    struct mutex operation_synchronizer;  // Lock sullo specifico device, per sincronizzare l'accesso di thread concorrenti
    stream_block *head;                   // Puntatore al primo blocco dati dello stream
    stream_block *tail;                   // Puntatore all' ultimo blocco dati dello stream. Permette di appendere più velocemente uno stream block
} object_state;

/**
 * Mantiene lo stato della sessione
 */
typedef struct _session_state {
    int blocking;  // Operazioni bloccanti o non-bloccanti
    int priority;  // Livello di priorità della sessione [0,1] = [low,high]
    int timeout;   // TODO implementare timeout
} session_state;