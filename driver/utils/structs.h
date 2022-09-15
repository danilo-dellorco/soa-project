/*
=====================================================================================================
                                            structs.h
-----------------------------------------------------------------------------------------------------
Mantiene tutte le strutture utilizzate all'interno del device driver
=====================================================================================================
*/

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
 * Mantinene tutte le informazioni sul singolo flusso di priorità
 */
typedef struct _flow_state {
    struct mutex operation_synchronizer;  // Lock sullo specifico device, per sincronizzare l'accesso di thread concorrenti
    stream_block *head;                   // Puntatore al primo blocco dati dello stream
    stream_block *tail;                   // Puntatore all' ultimo blocco dati dello stream. Permette di appendere più velocemente un nuovo stream block.
    wait_queue_head_t wait_queue;         // Wait Event Queue, mantiene i task bloccanti messi in sleep.
} flow_state;

/**
 * Mantinene lo stato del device
 */
typedef struct _object_state {
    long available_bytes;                 // Mantiene lo spazio libero totale del dispositivo, a prescindere dai due flussi.
    flow_state priority_flow[NUM_FLOWS];  // Mantiene lo stato complessivo del flusso ad alta e bassa priorità
} object_state;

/**
 * Mantiene lo stato della sessione
 */
typedef struct _session_state {
    int blocking;  // Operazioni bloccanti o non-bloccanti [0,1] = [blocking,non-blocking]
    int priority;  // Livello di priorità della sessione [0,1] = [low,high]
    int timeout;   // Timeout per il risveglio dei thread in wait_queue [>0]
} session_state;

/**
 *  Struttura utilizzata nel meccanismo di deferred work
 */
typedef struct _packed_work_struct {
    const char *data;         // Puntatore al buffer kernel temporaneo dove sono salvati i dati da scrivere poi sullo stream.
    stream_block *new_block;  // Puntatore al blocco vuoto per la scrittura successiva.
    int minor;                // Minor number del device su cui si sta operando.
    size_t len;               // Quantità di dati da scrivere, corrisponde alla lunghezza del buffer 'data'.
    session_state *session;   // Puntatore alla session_state verso il device su cui effettuare la scrittura.
    struct work_struct work;  // Struttura di deferred work
} packed_work_struct;

#endif