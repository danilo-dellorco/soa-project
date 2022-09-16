/*
=====================================================================================================
                                            multiflow_driver.c
-----------------------------------------------------------------------------------------------------
Implementa l'effettivo Multi-flow device driver
=====================================================================================================
*/

#include <linux/delay.h>

#include "utils/params.h"
#include "utils/structs.h"
#include "utils/tools.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Danilo Dell'Orco");

/*
 * Prototipi delle funzioni del driver
 */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

ssize_t write_on_stream(const char *buff, size_t len, flow_state *the_flow);
int schedule_write(const char *buff, size_t len, session_state *session, object_state *the_object);
void write_deferred(struct work_struct *deferred_work);

static int Major;
static int Minor;

/**
 * Dobbiamo gestire 128 dispositivi di I/O, quindi 128 minor numbers differenti.
 * Definiamo un array objects che mantiene 128 differenti strutture object_state.
 **/
object_state objects[NUM_DEVICES];

/*
 * Invocata dal VFS quando viene aperto il nodo associato al driver.
 */
static int dev_open(struct inode *inode, struct file *file) {
    session_state *session;

    printk("%s: ------------------------------------- OPEN -------------------------------------------\n", MODNAME);

    Minor = get_minor(file);
    if (Minor >= NUM_DEVICES || Minor < 0) {
        printk("%s: minor %d not in (0,%d).\n", MODNAME, Minor, NUM_DEVICES - 1);
        return OPEN_ERROR;
    }

    if (device_enabling[Minor] == DISABLED) {
        printk("%s: device with minor %d is disabled, and cannot be opened.\n", MODNAME, Minor);
        return OPEN_ERROR;
    }

    session = kzalloc(sizeof(session_state), GFP_ATOMIC);
    if (session == NULL) {
        printk("%s: kzalloc error, unable to allocate session\n", MODNAME);
        return OPEN_ERROR;
    }

    // Parametri di default per la nuova sessione
    session->priority = HIGH_PRIORITY;
    session->blocking = NON_BLOCKING;
    session->timeout = 0;
    file->private_data = session;
    printk(KERN_INFO "%s: Session state %d correctly allocated.\n", MODNAME, current->pid);

    printk("%s: Process %d successfully opened the device file with Minor %d\n", MODNAME, current->pid, Minor);
    return 0;
}

/**
 * Invocata dal VFS quando si chiude il file.
 */
static int dev_release(struct inode *inode, struct file *file) {
    printk("%s: ------------------------------------- CLOSE -------------------------------------------\n", MODNAME);

    kfree(file->private_data);
    printk(KERN_INFO "%s: Session state %d correctly deallocated.\n", MODNAME, current->pid);
    printk("%s: Device file %d closed by process %d\n", MODNAME, Minor, current->pid);
    return 0;
}

// ------------------------------------------ WRITE OPERATION ----------------------------------------------
/**
 * Implementazione dell'operazione di scrittura del driver.
 *  - Per le operazioni a bassa priorità viene invocata la write_low che utilizza il meccanismo di deferred work. Il risultato della write viene
 *    comunque notificato in modo sincrono: per questo si verifica subito se c'è spazio sufficiente per la scrittura e viene subito aggiornato lo spazio rimanente.
 *
 *  - Per le operazioni ad alta priorità viene chiamata direttamente la write_on_stream, che internamente cerca di prendere il lock ed effettua la scrittura effettiva sul flusso.
 */
static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off) {
    session_state *session = filp->private_data;
    int priority = session->priority;
    int blocking = session->blocking;
    ssize_t written_bytes = 0;
    int lock;

    object_state *the_object;
    flow_state *the_flow;
    the_object = &objects[Minor];
    the_flow = &the_object->priority_flow[priority];

    printk("%s: ------------------------------------- WRITE -------------------------------------------\n", MODNAME);
    printk(KERN_INFO "%s: Called a %s %s write on dev [%d,%d]\n", MODNAME, get_prio_str(priority), get_block_str(blocking), Major, Minor);
    printk(KERN_INFO "%s: Write size: %ld bytes | Free space: %ld bytes\n", MODNAME, len, the_object->available_bytes);

    lock = get_lock(the_object, session, Minor, TRYLOCK);

    if (lock == LOCK_NOT_ACQUIRED) {
        printk("%s: Write error, unable to get lock on dev [%d,%d].\n", MODNAME, Major, Minor);
        return WRITE_ERROR;
    }

    if (len > the_object->available_bytes) {
        printk("%s: Write error, there is no enough space on dev [%d,%d].\n", MODNAME, Major, Minor);
        release_lock(the_flow);
        return WRITE_ERROR;
    }

    // Ad alta priorità viene chiamata direttamente la write_on_stream, dopo aver ottenuto il lock e controllato che lo spazio sia sufficiente.
    if (priority == HIGH_PRIORITY) {
        written_bytes = write_on_stream(buff, len, the_flow);

#ifdef TEST
        printk(KERN_DEBUG "%s: [TEST] waiting %d msec to release write lock\n", MODNAME, TEST_TIME);
        msleep(TEST_TIME);
#endif
    }

    // Nel flusso a bassa priorità si notifica subito all'utente se la scrittura può avvenire o meno. La write viene schedulata tramite deferred work, e per costruzione non può fallire.
    else if (priority == LOW_PRIORITY) {
        written_bytes = schedule_write(buff, len, session, the_object);
    }
    release_lock(the_flow);
    printk("%s: ---------------------------------------------------------------------------------------\n", MODNAME);
    return written_bytes;
}

/**
 * Esegue la scrittura effettiva sullo stream di dati, scegliendo il flusso specifico in base alla priorità della scrittura.
 */
ssize_t write_on_stream(const char *buff, size_t len, flow_state *the_flow) {
    stream_block *current_block;
    stream_block *empty_block;
    char *block_buff;
    int ret;

    // Allocazione delle strutture necessarie alla scrittura
    block_buff = kzalloc(len + 1, GFP_ATOMIC);
    if (block_buff == NULL) {
        printk("%s: Data buffer allocation error.\n", MODNAME);
        return WRITE_ERROR;
    }
    empty_block = kzalloc(sizeof(stream_block), GFP_ATOMIC);
    if (packed_work->new_block == NULL) {
        printk("%s: New block allocation error.\n", MODNAME);
        return SCHED_ERROR;
    }

    // Copia dei bytes da scrivere nel stream_block del dispositivo.
    ret = copy_from_user(block_buff, buff, len);
    current_block = the_flow->tail;
    current_block->stream_content = block_buff;

    // Creazione di un blocco vuoto per la scrittura successiva a quella attuale. Il blocco viene messo in coda allo stream.
    empty_block->next = NULL;
    empty_block->stream_content = NULL;
    empty_block->id = current_block->id + 1;

    current_block->next = empty_block;
    the_flow->tail = empty_block;

    // Aggiornamento del numero di bytes disponibili
    the_object->available_bytes -= (len - ret);
    total_bytes_high[Minor] += (len - ret);
    printk("%s: Written %ld/%ld bytes in block %d: '%s'\n", MODNAME, strlen(current_block->stream_content), len, current_block->id, current_block->stream_content);
    return len - ret;
}

/**
 * Schedula la scrittura sul flusso a bassa priorità. Copia i dati utente da scrivere in un buffer kernel temporaneo,
 * che verrà immesso effettivamente nello stream soltanto quando verrà schedulata la write_deferred.
 */
int schedule_write(const char *buff, size_t len, session_state *session, object_state *the_object) {
    int ret;
    packed_work_struct *packed_work;
    printk("%s: Deferred work requested.\n", MODNAME);

    packed_work = kzalloc(sizeof(packed_work_struct), GFP_ATOMIC);
    if (packed_work == NULL) {
        printk("%s: Packed work_struct allocation failure\n", MODNAME);
        return SCHED_ERROR;
    }

    packed_work->len = len;
    packed_work->session = session;
    packed_work->minor = Minor;

    // Allocazione delle strutture per effettuare la scrittura successivamente
    packed_work->data = kzalloc(len + 1, GFP_ATOMIC);
    if (packed_work->data == NULL) {
        printk("%s: Packed work_struct data allocation failure\n", MODNAME);
        return SCHED_ERROR;
    }
    packed_work->new_block = kzalloc(sizeof(stream_block), GFP_ATOMIC);
    if (packed_work->new_block == NULL) {
        printk("%s: Packed work_struct new block allocation failure\n", MODNAME);
        return SCHED_ERROR;
    }

    // Copia dei dati da scrivere nel buffer
    ret = copy_from_user((char *)packed_work->data, buff, len);
    if (ret != 0) {
        printk("%s: Packed work_struct data copy failure.\n", MODNAME);
        return SCHED_ERROR;
    }

    // Riservo logicamente lo spazio libero sul dispositivo
    the_object->available_bytes -= written_bytes;
    total_bytes_low[Minor] += written_bytes;

    printk(KERN_INFO "%s: Packed work_struct correctly allocated.\n", MODNAME);

    // Inizializza la work_struct nella struttura packed, specificando come lavoro da eseguire la write_deferred
    __INIT_WORK(&(packed_work->work), &write_deferred, (unsigned long)&(packed_work->work));
    schedule_work(&packed_work->work);

    return len - ret;
}

/**
 * Funzione associata alla work_struct in __INIT_WORK, per effettuare la scrittura in modalità deferred.
 */
void write_deferred(struct work_struct *deferred_work) {
    // Ottenimento del lock tramite mutex_lock
    printk("%s: kworker daemon with PID=%d is processing the deferred write operation.\n", MODNAME, current->pid);
    get_lock(the_object, session, minor, LOCK);

    packed_work_struct *packed = container_of(deferred_work, packed_work_struct, work);
    int minor = packed->minor;
    session_state *session = packed->session;
    object_state *the_object = &objects[minor];
    flow_state *the_flow = &the_object->priority_flow[LOW_PRIORITY];
    size_t len = packed->len;

    stream_block *current_block;
    stream_block *empty_block;

    // Si scrivono i dati sul flusso
    current_block = the_flow->tail;
    current_block->stream_content = (char *)packed->data;

    // Si inizializza il blocco vuoto
    empty_block = packed->new_block;
    empty_block->next = NULL;
    empty_block->stream_content = NULL;
    empty_block->id = current_block->id + 1;

    // Si aggiunge il blocco vuoto in coda allo stream.
    current_block->next = empty_block;
    the_flow->tail = empty_block;

    printk("%s: Written %ld/%ld bytes in block %d: '%s'\n", MODNAME, strlen(current_block->stream_content), len, current_block->id, current_block->stream_content);

    kfree(packed);
    release_lock(the_flow);
}

// ------------------------------------------ READ OPERATION ----------------------------------------------
/**
 * Implementazione dell'operazione di lettura del driver. Questa avviene in un while(1) leggendo progressivamente i blocchi dello stream.
 * - Se la dimensione della read va a leggere completamente i bytes di un blocco si libera la rispettiva area di memoria e si passa al blocco successivo.
 * - Se la lettura non consuma totalmente i bytes di un blocco si aggiorna soltanto l'offset sulla posizione attuale.
 */
static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {
    int ret;
    int cls;
    int block_residual;
    long block_size;
    int to_read;
    int bytes_read;
    int old_id;
    object_state *the_object;
    flow_state *the_flow;
    stream_block *current_block;
    stream_block *completed_block;
    session_state *session = filp->private_data;

    int priority = session->priority;
    int blocking = session->blocking;

    cls = clear_user(buff, len);

    the_object = &objects[Minor];
    the_flow = &the_object->priority_flow[priority];
    printk("%s: -------------------------------------- READ -------------------------------------------\n", MODNAME);
    printk(KERN_INFO "%s: Called a %s %s read of %ld bytes on dev [%d,%d]\n", MODNAME, get_prio_str(priority), get_block_str(blocking), len, Major, Minor);

    // Ottenimento del lock. In base al tipo di operazione blocking/non-blocking si attende o meno.
    ret = get_lock(the_object, session, Minor, TRYLOCK);
    if (ret < 0) {
        return READ_ERROR;
    }

    current_block = the_flow->head;
    printk(KERN_INFO "%s: Start reading from head - block%d \n", MODNAME, current_block->id);

    // Non sono presenti dati nello stream, quindi viene rilasciato il lock e si ritorna al chiamante.
    if (current_block->stream_content == NULL) {
        printk("%s: No data to read in current block\n", MODNAME);
        release_lock(the_flow);
        return READ_ERROR;
    }

    to_read = len;
    bytes_read = 0;

    // Ciclo in cui vengono letti i bytes richiesti dallo stream.
    while (1) {
        block_size = strlen(current_block->stream_content);
        printk(KERN_INFO "%s: Read iteration -> [to_read: %d, block_size: %ld, read_off: %d, bytes_read: %d]\n", MODNAME, to_read, block_size, current_block->read_offset, bytes_read);

        // Richiesta la lettura di più byte rispetto a quelli da leggere nel blocco corrente.
        if (block_size - current_block->read_offset < to_read) {
            printk(KERN_INFO "%s: Read | Full reading in block%d", MODNAME, current_block->id);
            block_residual = block_size - current_block->read_offset;
            to_read -= block_residual;
            ret = copy_to_user(&buff[bytes_read], &(current_block->stream_content[current_block->read_offset]), block_residual);
            bytes_read += (block_residual - ret);

            // Sposto logicamente l'inizio dello stream al blocco successivo.
            old_id = current_block->id;
            completed_block = current_block;
            current_block = current_block->next;
            the_flow->head = current_block;

            // Sono stati letti tutti i dati dal blocco precedente, quindi posso liberare la rispettiva area di memoria.
            kfree(completed_block->stream_content);
            kfree(completed_block);
            printk(KERN_INFO "%s: Read | Block%d fully read. Memory released.", MODNAME, current_block->id);

            // Siamo nell'ultimo blocco dello stream e sono stati quindi letti tutti i byte disponibili. Si sblocca il mutex e si ritorna al chiamante senza passare al blocco successivo.
            if (current_block->stream_content == NULL) {
                if (priority == HIGH_PRIORITY) {
                    total_bytes_high[Minor] -= bytes_read;
                } else {
                    total_bytes_low[Minor] -= bytes_read;
                }
                printk("%s: Read completed (1), read %d bytes\n", MODNAME, bytes_read);
                the_flow->head = current_block;
                the_flow->tail = current_block;
                the_object->available_bytes += bytes_read;
                release_lock(the_flow);
                printk("%s: ---------------------------------------------------------------------------------------\n", MODNAME);

                return bytes_read;
            }
        }

        // Il numero di byte richiesti sono presenti nel blocco corrente. Si copiano i byte nel buffer utente, si sblocca il mutex e si ritorna al chiamante.
        else {
            printk(KERN_INFO "%s: Partial reading in block%d\n", MODNAME, current_block->id);
            ret = copy_to_user(&buff[bytes_read], &(current_block->stream_content[current_block->read_offset]), to_read);
            bytes_read += (to_read - ret);
            current_block->read_offset += (to_read - ret);
            if (priority == HIGH_PRIORITY) {
                total_bytes_high[Minor] -= bytes_read;
            } else {
                total_bytes_low[Minor] -= bytes_read;
            }
            printk("%s: Read completed (2), read %d bytes\n", MODNAME, bytes_read);
            release_lock(the_flow);
            the_object->available_bytes += bytes_read;
            printk("%s: ---------------------------------------------------------------------------------------\n", MODNAME);
            return bytes_read;
        }
    }
}

/**
 * Permette di controllare i parametri della sessione di I/O
 * 3)  Switch to HIGH priority
 * 4)  Switch to LOW priority
 * 5)  Use BLOCKING operations
 * 6)  Use NON-BLOCKING
 * 7)  Set timeout
 * 8)  Enable a device file  [UNUSED]
 * 9)  Disable a device file [UNUSED]
 */
static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param) {
    session_state *session;
    session = filp->private_data;

    switch (command) {
        case SET_LOW_PRIORITY:
            session->priority = LOW_PRIORITY;
            printk(
                "%s: ioctl(%u) | thread %d has set priority level to LOW on [%d,%d]\n",
                MODNAME, command, current->pid, Major, Minor);
            break;
        case SET_HIGH_PRIORITY:
            session->priority = HIGH_PRIORITY;
            printk(
                "%s: ioctl(%u) | thread %d has set priority level to HIGH on [%d,%d]\n",
                MODNAME, command, current->pid, Major, Minor);
            break;
        case SET_BLOCKING_OP:
            session->blocking = BLOCKING;
            printk(
                "%s: ioctl(%u) | thread %d has set operation type to BLOCKING on [%d,%d]\n",
                MODNAME, command, current->pid, Major, Minor);
            break;
        case SET_NON_BLOCKING_OP:
            session->blocking = NON_BLOCKING;
            printk(
                "%s: ioctl(%u) | thread %d has set operation type to NON-BLOCKING on [%d,%d]\n",
                MODNAME, command, current->pid, Major, Minor);
            break;
        case SET_TIMEOUT:
            session->timeout = param;
            printk(
                "%s: ioctl(%u) | thread %d has changed the TIMEOUT value on [%d,%d]\n",
                MODNAME, command, current->pid, Major, Minor);
            break;
        // Implementation of enable/disable device using ioctl instead of writing to file
        // case ENABLE_DEV:
        //     device_enabling[Minor] = ENABLED;
        //     printk(
        //         "%s: device [%d,%d] has been ENABLED \n",
        //         MODNAME, Major, Minor, command);
        //     break;
        // case DISABLE_DEV:
        //     device_enabling[Minor] = DISABLED;
        //     printk(
        //         "%s: device [%d,%d] has been DISABLED \n",
        //         MODNAME, Major, Minor, command);
        //     break;
        default:
            printk(
                "%s: ioctl(%u) | thread %d has used an illegal command on [%d,%d]\n",
                MODNAME, command, current->pid, Major, Minor);
    }
    return 0;
}

/*
 * Definizione delle file_operations del Driver.
 */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = dev_write,
    .read = dev_read,
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = dev_ioctl};

/*
 *  Inizializza tutti i dispositivi e registra il Char Device nel kernel. Fornisce inoltre tramite printk il Major Number che viene assegnato al Driver.
 */
int init_module(void) {
    int i, j;
    printk("%s: -------------------------------------- INIT -------------------------------------------\n", MODNAME);

    // Inizializzazione dei dispositivi
    printk(KERN_INFO "%s: Initializing Object State.\n", MODNAME);
    for (i = 0; i < NUM_DEVICES; i++) {
        for (j = 0; j < NUM_FLOWS; j++) {
            flow_state *object_flow = &objects[i].priority_flow[j];
            mutex_init(&(object_flow->operation_synchronizer));

            // Allocazione per il primo blocco dello stream
            object_flow->head = kzalloc(sizeof(stream_block), GFP_KERNEL);
            object_flow->head->id = 0;
            object_flow->tail = object_flow->head;
            object_flow->head->next = NULL;
            object_flow->head->stream_content = NULL;
            object_flow->head->read_offset = 0;

            // Inizializzazione della waitqueue
            init_waitqueue_head(&object_flow->wait_queue);

            if (object_flow->head == NULL) goto revert_allocation;
        }

        // Di default tutti i dispositivi sono abilitati
        device_enabling[i] = ENABLED;
        objects[i].available_bytes = MAX_SIZE_BYTES;
    }
    printk(KERN_INFO "%s: Object State correctly Initialized.\n", MODNAME);

    // Registrazione del Char Device Driver
    Major = __register_chrdev(0, 0, 128, DEVICE_NAME, &fops);
    if (Major < 0) {
        printk("%s: registering device failed\n", MODNAME);
        return Major;
    }
    printk("%s: New device registered, it is assigned major number %d\n", MODNAME, Major);
    printk("%s: ---------------------------------------------------------------------------------------\n", MODNAME);

    return 0;

revert_allocation:
    printk(KERN_INFO "Error allocating stream head block. Revert allocation\n");
    for (; i >= 0; i--) {
        kfree(&objects[i].priority_flow[j].head);
    }
    return -ENOMEM;
}

/**
 * Effettua il cleanup del modulo quando questo viene smontato/deregistrato
 */
void cleanup_module(void) {
    int i = 0;
    int j = 0;
    printk("%s: ------------------------------------- CLEAN -------------------------------------------\n", MODNAME);
    printk(KERN_INFO "%s: Unregistering the device, releasing pending resources.\n", MODNAME);

    // Rilascio delle risorse
    for (i = 0; i < NUM_DEVICES; i++) {
        for (j = 0; j < NUM_FLOWS; j++) {
            flow_state *object_flow = &objects[i].priority_flow[j];
            stream_block *current_block = object_flow->head;

            // Deallocazione di tutti i blocchi dati dello stream.
            while (current_block->stream_content != NULL) {
                kfree(current_block->stream_content);
                current_block = current_block->next;
            }
        }
    }
    printk(KERN_INFO "%s: Data stream memory released.\n", MODNAME);

    // Deregistrazione del Device.
    unregister_chrdev(Major, DEVICE_NAME);
    printk("%s: The device with major number %d has been unregistered.\n", MODNAME, Major);
    printk("%s: ---------------------------------------------------------------------------------------\n", MODNAME);
    return;
}