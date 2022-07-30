#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pid.h> /* For pid types */
#include <linux/sched.h>
#include <linux/tty.h>     /* For the tty declarations */
#include <linux/version.h> /* For LINUX_VERSION_CODE */

/**
 * Blocco dati relativo allo stream
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
    int valid_bytes;                      // TODO valutare l'utilità
    stream_block *head;                   // Puntatore al primo blocco dati dello stream
    stream_block *tail;                   // Puntatore all' ultimo blocco dati dello stream. Permette di appendere più velocemente uno stream block
} object_state;
