/*
=====================================================================================================
                                            tools.h
-----------------------------------------------------------------------------------------------------
Funzioni e macro ausiliarie utilizzate nel device driver
=====================================================================================================
*/
// TODO problema con due thread high priority e quello bloccante muore invece di aspettare

#include "params.h"

/**
 * Definisco le macro per ottenere MAJOR e MINOR number dalla sessione corrente, in base alla versione del kernel utilizzata
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session) MAJOR(session->f_inode->i_rdev)
#define get_minor(session) MINOR(session->f_inode->i_rdev)
#else
#define get_major(session) MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session) MINOR(session->f_dentry->d_inode->i_rdev)
#endif

/**
 * Inserisce un task in waitqueue, cercando di ottenere il lock ad ogni wake_up() fino allo scadere del timeout.
 * Ritorna 1 se il lock è stato acquisito, 0 se il timeout è scaduto senza acquisire il lock
 */
int put_to_waitqueue(unsigned long timeout, struct mutex *mutex, wait_queue_head_t *wq) {
    int val;
    if (timeout == 0) {
        return 0;
    }

    printk("%s: put_to_waitqueue | thread with pid %d will sleep for %lu ms\n", MODNAME, current->pid, timeout);

    // Si mette in sleep il processo finché la condizione non è True. La condizione viene verificata ogni volta che la waitqueue viene risvegliata.
    // Ritorna il numero di jiffies rimenenti (>=1) se la condizione viene verificata, 0 se resta False fino allo scadere del timeout
    val = wait_event_timeout(*wq, mutex_trylock(mutex), msecs_to_jiffies(timeout));

    printk("%s: put_to_waitqueue | thread with pid %d awaken\n", MODNAME, current->pid);

    // Non è stato acquisito il lock
    if (val == 0) {
        printk("put_to_waitqueue | thread %d timeout elapsed. lock not acquired\n", current->pid);
        return 0;
    }
    printk("put_to_waitqueue | thread %d lock acquired\n", current->pid);

    return 1;
}

/**
 * Prova ad acquisire il lock sul mutex.
 * Se l'operazione è non bloccante e il lock non viene acquisito, viene ritornato NULL e l'operazione fallisce.
 * Se l'operazione è bloccante ed il lock non viene acquisito, il task viene messo nella waitqueue.
 * Ritorna 0 se il lock viene acquisito correttamente, -1 se il lock non viene acquisito allo scadere del timeout, -2 se il lock non viene acquisito in una operazione non-bloccante.
 */
int try_mutex_lock(object_state *the_object, session_state *session, int minor, int op) {
    int lock;
    int ret;
    wait_queue_head_t *wq;
    wq = &the_object->wait_queue;

    // Una scrittura low priority non può fallire, quindi il processo attende attivamente di ottenere il lock.
    if (session->priority == LOW_PRIORITY && op == WRITE_OP) {
        printk("%s: try_mutex_lock | Low Priority - Process %d waiting to get lock.\n", MODNAME, current->pid);
        __sync_fetch_and_add(&waiting_threads_low[minor], 1);  // Necessario per evitare out-of-order
        mutex_lock(&(the_object->operation_synchronizer));
        __sync_fetch_and_add(&waiting_threads_low[minor], -1);
        printk("%s: try_mutex_lock | Low Priority - Process %d acquired lock.\n", MODNAME, current->pid);
        return 0;
    }

    // Operazioni HIGH_PRIORITY
    lock = mutex_trylock(&(the_object->operation_synchronizer));

    if (lock == 0) {
        printk("%s: Unable to get lock.\n", MODNAME);
        if (session->blocking == BLOCKING) {
            printk("%s: Attempt to get lock. Timeout of %d ms\n", MODNAME, session->timeout);

            waiting_threads_high[minor]++;
            ret = put_to_waitqueue(session->timeout, &the_object->operation_synchronizer, wq);
            waiting_threads_high[minor]--;

            // Sessione bloccante, ma lock non acquisito a timeout scaduto
            if (ret == 0) {
                // printk("%s: Timeout expired, lock not acquired.\n", MODNAME);
                return -1;
            }
        }

        // Sessione non bloccante e lock non acquisito
        else {
            printk("%s: Non-blocking operation, lock failed.\n", MODNAME);
            return -2;
        }
    }

    printk("%s: Lock succesfully acquired.\n", MODNAME);
    return 0;
}