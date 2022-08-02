#ifndef STRUCTS_H
#define STRUCTS_H
#include "params.h"

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
    stream_block *tail;                   // Puntatore all' ultimo blocco dati dello stream. Permette di appendere più velocemente un nuovo stream block.
    wait_queue_head_t wait_queue;         // Wait Event Queue, mantiene i task bloccanti messi in sleep.
} object_state;

/**
 * Mantiene lo stato della sessione
 */
typedef struct _session_state {
    int blocking;  // Operazioni bloccanti o non-bloccanti
    int priority;  // Livello di priorità della sessione [0,1] = [low,high]
    int timeout;   // TODO implementare timeout
} session_state;

#endif