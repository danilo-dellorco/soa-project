/*
=====================================================================================================
                                            tools.h
-----------------------------------------------------------------------------------------------------
Funzioni e macro ausiliarie utilizzate nel device driver
=====================================================================================================
*/

#include "params.h"

/**
 * Macro per ottenere MAJOR e MINOR number dalla sessione corrente, in base alla versione del kernel utilizzata
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

    printk(KERN_INFO "%s: Thread %d will sleep for %lu ms\n", MODNAME, current->pid, timeout);

    // Si mette in sleep il processo finché la condizione non è True. La condizione viene verificata ogni volta che la waitqueue viene risvegliata.
    // Ritorna il numero di jiffies rimenenti (>=1) se la condizione viene verificata, 0 se resta False fino allo scadere del timeout
    val = wait_event_timeout(*wq, mutex_trylock(mutex), msecs_to_jiffies(timeout));

    printk(KERN_INFO "%s: Thread %d awaken\n", MODNAME, current->pid);

    // Non è stato acquisito il lock
    if (val == 0) {
        printk("%s: Thread %d timeout elapsed. Lock not acquired\n", MODNAME, current->pid);
        return 0;
    }
    printk("%s: Thread %d lock succesfully acquired\n", MODNAME, current->pid);

    return 1;
}

/**
 * Prova ad acquisire il lock sul mutex. Il comportamento varia a seconda del tipo di operazione.
 * - Se l'operazione è una scrittura low priority si usa mutex_lock per attendere di prendere il lock.
 * - Se l'operazione è non bloccante e il lock non viene acquisito nel trylock, l'operazione fallisce.
 * - Se l'operazione è bloccante ed il lock non viene acquisito, il task viene messo nella waitqueue.
 * Ritorna 0 se il lock viene acquisito correttamente, -1 se il lock non viene acquisito.
 */
int get_lock(object_state *the_object, session_state *session, int minor, int lock_type) {
    int lock;
    int ret;
    int priority;
    wait_queue_head_t *wq;
    flow_state *the_flow;
    priority = session->priority;
    the_flow = &the_object->priority_flow[priority];
    wq = &the_flow->wait_queue;

    // Una scrittura low priority non può fallire, quindi il processo attende attivamente di ottenere il lock prima della write_on_stream.
    if (lock_type == LOCK) {
        printk(KERN_INFO "%s: Process %d actively waiting to get lock.\n", MODNAME, current->pid);
        __sync_fetch_and_add(&waiting_threads_low[minor], 1);
        mutex_lock(&(the_flow->operation_synchronizer));
        __sync_fetch_and_add(&waiting_threads_low[minor], -1);
        printk(KERN_INFO "%s: Process %d acquired lock.\n", MODNAME, current->pid);
        return LOCK_ACQUIRED;
    }

    // Operazioni sincrone, si effettua il trylock
    lock = mutex_trylock(&(the_flow->operation_synchronizer));

    if (lock == 0) {
        printk("%s: Lock not available.\n", MODNAME);
        if (session->blocking == BLOCKING) {
            printk(KERN_INFO "%s: Blocking operation, attempt to get lock.\n", MODNAME);

            waiting_threads_high[minor]++;
            ret = put_to_waitqueue(session->timeout, &the_flow->operation_synchronizer, wq);
            waiting_threads_high[minor]--;

            // Sessione bloccante, ma lock non acquisito a timeout scaduto
            if (ret == 0) {
                return LOCK_NOT_ACQUIRED;
            }
        }
        // Sessione non bloccante e lock non acquisito
        else {
            printk("%s: Non-blocking operation, lock failed.\n", MODNAME);
            return LOCK_NOT_ACQUIRED;
        }
    }

    printk(KERN_INFO "%s: Lock succesfully acquired.\n", MODNAME);
    return LOCK_ACQUIRED;
}

/**
 * Rilascia il mutex del flusso passato in input, e sveglia la relativa waitqueue.
 */
void release_lock(flow_state *the_flow) {
    mutex_unlock(&(the_flow->operation_synchronizer));
    wake_up(&the_flow->wait_queue);
    printk(KERN_INFO "%s: Lock succesfully released.\n", MODNAME);
}