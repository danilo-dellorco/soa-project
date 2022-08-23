/*
=====================================================================================================
                                            multiflow-driver.c
-----------------------------------------------------------------------------------------------------
Implementa l'effettivo Multi-flow device driver
=====================================================================================================
*/

#include <linux/delay.h>

#include "utils/params.h"
#include "utils/structs.h"
#include "utils/tools.h"

#define TEST

// TODO qualche bug nella lettura high di bytes superiori, settano a -1 tipo.

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Danilo Dell'Orco");

/*
 * Prototipi delle funzioni del driver
 */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

ssize_t write_on_stream(const char *buff, size_t len, session_state *session, object_state *the_object);
int write_low(const char *buff, size_t len, session_state *session, object_state *the_object);
void write_deferred(struct work_struct *deferred_work);

static int Major;
static int Minor;

/**
 * Dobbiamo gestire 128 dispositivi di I/O, quindi 128 minor numbers differenti.
 * Definiamo quindi 128 differenti strutture object_state mantenute nell'array objects.
 * Per ogni device ci sono 2 strutture object_state, per ognuno dei due flussi di priorità
 **/
object_state objects[NUM_DEVICES][NUM_FLOWS];

/*
 * Invocata dal VFS quando apriamo il nodo associato al driver.
 */
static int dev_open(struct inode *inode, struct file *file) {
    session_state *session;

    Minor = get_minor(file);

    if (Minor >= NUM_DEVICES || Minor < 0) {
        printk("%s: minor %d not in (0,%d).\n", MODNAME, Minor, NUM_DEVICES - 1);
        return -ENODEV;
    }

    if (device_enabling[Minor] == DISABLED) {
        printk("%s: device with minor %d is disabled, and cannot be opened.\n", MODNAME, Minor);
        return -2;
    }

    session = kzalloc(sizeof(session_state), GFP_ATOMIC);
    if (session == NULL) {
        printk("%s: kzalloc error, unable to allocate session\n", MODNAME);
        return -ENOMEM;
    }

    // Parametri di default per la nuova sessione
    session->priority = HIGH_PRIORITY;
    session->blocking = NON_BLOCKING;
    session->timeout = 0;
    file->private_data = session;

    printk("%s: device file successfully opened for object with Minor %d\n", MODNAME, Minor);
    return 0;
}

/**
 * Invocata dal VFS quando si chiude il file, quindi la open è già stata effettuata correttamente
 */
static int dev_release(struct inode *inode, struct file *file) {
    session_state *session = file->private_data;
    kfree(session);

    printk("%s: device file %d closed\n", MODNAME, Minor);
    return 0;
}

/**
 * Funzione eseguita effettivamente quando andiamo a chiamare una write() a livello applicativo
 * @param filp puntatore alla struttura dati che rappresenta la sessione
 * @param buff puntatore all'area di memoria applicativa passato dalla syscall
 * @param len taglia della write
 * @param off offset all'area di memoria filp su cui lavoriamo.
 */
static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off) {
    session_state *session = filp->private_data;
    int priority = session->priority;
    int blocking = session->blocking;
    ssize_t written_bytes = 0;

    object_state *the_object;
    the_object = &objects[Minor][priority];
    printk("%s: somebody called a %s %s write of %ld bytes on dev [%d,%d]\n", MODNAME, get_prio_str(priority), get_block_str(blocking), len, Major, Minor);

    if (priority == LOW_PRIORITY) {
        written_bytes = write_low(buff, len, session, the_object);
        total_bytes_low[Minor] += written_bytes;

    } else if (priority == HIGH_PRIORITY) {
        written_bytes = write_on_stream(buff, len, session, the_object);
        total_bytes_high[Minor] += written_bytes;
    }

    return written_bytes;
}

/**
 * Funzione eseguita effettivamente quando andiamo a chiamare una read() a livello applicativo
 * @param filp puntatore alla struttura dati che rappresenta la sessione
 * @param buff puntatore all'area di memoria applicativa passato dalla syscall
 * @param len taglia della write
 * @param off offset all'area di memoria filp su cui lavoriamo.
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
    stream_block *current_block;
    stream_block *completed_block;
    session_state *session = filp->private_data;

    int priority = session->priority;
    int blocking = session->blocking;

    cls = clear_user(buff, len);

    the_object = &objects[Minor][priority];
    printk("%s: somebody called a %s %s read of %ld bytes on dev [%d,%d]\n", MODNAME, get_prio_str(priority), get_block_str(blocking), len, Major, Minor);

    ret = try_mutex_lock(the_object, session, Minor, READ_OP);
    if (ret < 0) {
        return -EAGAIN;
    }

    current_block = the_object->head;
    printk("%s: start reading from head - block%d \n", MODNAME, current_block->id);

    // Non sono presenti dati nello stream
    if (current_block->stream_content == NULL) {
        printk("%s: No data to read in current block\n", MODNAME);
        mutex_unlock(&(the_object->operation_synchronizer));
        wake_up(&the_object->wait_queue);
        return 0;
    }

    to_read = len;
    bytes_read = 0;

    while (1) {
        block_size = strlen(current_block->stream_content);
        printk("%s: to_read: %d, block_size: %ld, read_off: %d, bytes_read: %d\n", MODNAME, to_read, block_size, current_block->read_offset, bytes_read);

        // Richiesta la lettura di più byte rispetto a quelli da leggere nel blocco corrente
        if (block_size - current_block->read_offset < to_read) {
            printk("%s: cross block read in block %d", MODNAME, current_block->id);
            block_residual = block_size - current_block->read_offset;
            to_read -= block_residual;
            ret = copy_to_user(&buff[bytes_read], &(current_block->stream_content[current_block->read_offset]), block_residual);
            bytes_read += (block_residual - ret);

            printk("%s: block %d fully read.", MODNAME, current_block->id);
            // Sposto logicamente l'inizio dello stream al blocco successivo.
            old_id = current_block->id;
            completed_block = current_block;
            current_block = current_block->next;
            the_object->head = current_block;

            printk("--- head | %d -> %d ", old_id, the_object->head->id);

            // Sono stati letti tutti i dati dal blocco precedente, quindi posso liberare la rispettiva area di memoria.
            kfree(completed_block->stream_content);
            kfree(completed_block);

            printk("--- free | deallocated block %d memory", old_id);

            // Siamo nell'ultimo blocco, quindi abbiamo letto tutti i dati dello stream
            if (current_block->stream_content == NULL) {
                if (priority == HIGH_PRIORITY) {
                    total_bytes_high[Minor] -= bytes_read;
                } else {
                    total_bytes_low[Minor] -= bytes_read;
                }
                printk("%s: read completed (1), read %d bytes\n", MODNAME, bytes_read);
                the_object->head = current_block;
                the_object->tail = current_block;
                mutex_unlock(&(the_object->operation_synchronizer));
                wake_up(&the_object->wait_queue);
                return bytes_read;
            }
        }

        // Il numero di byte richiesti sono presenti nel blocco corrente
        else {
            printk("%s: whole block read in block %d| block content: '%s'", MODNAME, current_block->id, &current_block->stream_content[current_block->read_offset]);
            ret = copy_to_user(&buff[bytes_read], &(current_block->stream_content[current_block->read_offset]), to_read);
            bytes_read += (to_read - ret);
            current_block->read_offset += (to_read - ret);
            if (priority == HIGH_PRIORITY) {
                total_bytes_high[Minor] -= bytes_read;
            } else {
                total_bytes_low[Minor] -= bytes_read;
            }
            printk("%s: read completed (2), read %d bytes\n", MODNAME, bytes_read);
            mutex_unlock(&(the_object->operation_synchronizer));
            wake_up(&the_object->wait_queue);
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
 * 8)  Enable a device file
 * 9)  Disable a device file
 */
static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param) {
    session_state *session;
    session = filp->private_data;

    switch (command) {
        case 3:
            session->priority = LOW_PRIORITY;
            printk(
                "%s: ioctl(%u) | somebody has set priority level to LOW on [%d,%d]\n",
                MODNAME, command, Major, Minor);
            break;
        case 4:
            session->priority = HIGH_PRIORITY;
            printk(
                "%s: ioctl(%u) | somebody has set priority level to HIGH on [%d,%d]\n",
                MODNAME, command, Major, Minor);
            break;
        case 5:
            session->blocking = BLOCKING;
            printk(
                "%s: ioctl(%u) | somebody has set BLOCKING read & write on [%d,%d]\n",
                MODNAME, command, Major, Minor);
            break;
        case 6:
            session->blocking = NON_BLOCKING;
            printk(
                "%s: ioctl(%u) | somebody has set NON-BLOCKING read & write on [%d,%d]\n",
                MODNAME, command, Major, Minor);
            break;
        case 7:
            session->timeout = param;
            printk(
                "%s: ioctl(%u) | somebody has set TIMEOUT on [%d,%d]\n",
                MODNAME, command, Major, Minor);
            break;
        // Implementation of enable/disable device using ioctl instead of writing to file
        // case 8:
        //     device_enabling[Minor] = ENABLED;
        //     printk(
        //         "%s: somebody enabled the device [%d,%d] and command %u \n",
        //         MODNAME, Major, Minor, command);
        //     break;
        // case 9:
        //     device_enabling[Minor] = DISABLED;
        //     printk(
        //         "%s: somebody disabled the device [%d,%d] and command %u \n",
        //         MODNAME, Major, Minor, command);
        //     break;
        default:
            printk(
                "%s: somebody called an illegal command on [%d,%d]%u \n",
                MODNAME, Major, Minor, command);
    }
    return 0;
}

/*
 * Assegnazione della struttura driver, specificando i puntatori alle varie file operations implementate
 */

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = dev_write,
    .read = dev_read,
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = dev_ioctl};

/*
 *  Inizializza il modulo, scrivendo sul buffer del kernel il Major Number assegnato al Driver.
 */
int init_module(void) {
    int i, j;
    for (i = 0; i < NUM_DEVICES; i++) {
        for (j = 0; j < NUM_FLOWS; j++) {
            mutex_init(&(objects[i][j].operation_synchronizer));

            // Allocazione per il primo blocco di stream
            objects[i][j].head = kzalloc(sizeof(stream_block), GFP_KERNEL);
            objects[i][j].head->id = 0;
            objects[i][j].tail = objects[i][j].head;
            objects[i][j].head->next = NULL;
            objects[i][j].head->stream_content = NULL;
            objects[i][j].head->read_offset = 0;

            // Di default tutti i dispositivi sono abilitati
            init_waitqueue_head(&objects[i][j].wait_queue);

            if (objects[i][j].head == NULL) goto revert_allocation;
        }
        device_enabling[i] = ENABLED;
    }

    Major = __register_chrdev(0, 0, 128, DEVICE_NAME, &fops);

    if (Major < 0) {
        printk("%s: registering device failed\n", MODNAME);
        return Major;
    }

    printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n", MODNAME, Major);

    return 0;

revert_allocation:
    printk(KERN_INFO "revert allocation\n");
    for (; i >= 0; i--) {
        kfree(objects[i][j].head);
    }
    return -ENOMEM;
}

/**
 * Effettua il cleanup del modulo quando questo viene smontato/deregistrato
 *
 */
void cleanup_module(void) {
    unregister_chrdev(Major, DEVICE_NAME);

    printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n", MODNAME, Major);

    return;
}

ssize_t write_on_stream(const char *buff, size_t len, session_state *session, object_state *the_object) {
    stream_block *current_block;
    stream_block *empty_block;

    char *block_buff;
    int ret;

    // Blocco vuoto per la scrittura successiva. E' il blocco successivo a quello attualmente scritto
    empty_block = kzalloc(sizeof(stream_block), GFP_ATOMIC);
    block_buff = kzalloc(len + 1, GFP_ATOMIC);  // +1 per \n sui multipli di 8 bytes

    empty_block->next = NULL;
    empty_block->stream_content = NULL;

    printk("%s: stream write | to_write: %ld bytes\n", MODNAME, len);
    ret = try_mutex_lock(the_object, session, Minor, WRITE_OP);
    if (ret < 0) {
        return -EAGAIN;
    }

    if (session->priority == HIGH_PRIORITY) {
        ret = copy_from_user(block_buff, buff, len);
    } else if (session->priority == LOW_PRIORITY) {
        block_buff = (char *)buff;
        ret = 0;
    }
    current_block = the_object->tail;
    empty_block->id = current_block->id + 1;

    current_block->stream_content = block_buff;
    current_block->next = empty_block;
    the_object->tail = empty_block;

#ifdef TEST
    printk("%s: [TEST] waiting %d msec to release write lock\n", MODNAME, TEST_TIME);
    msleep(30000);
#endif
    mutex_unlock(&(the_object->operation_synchronizer));
    wake_up(&the_object->wait_queue);
    printk("%s: lock released, ret = %ld\n", MODNAME, len - ret);
    printk("%s: written %ld bytes in block %d: '%s'\n", MODNAME, strlen(current_block->stream_content), current_block->id, current_block->stream_content);
    return len - ret;
}

/**
 * Effettua la scrittura per il flusso a bassa priorità. Copia il buffer utente in un buffer kernel temporaneo,
 * che verrà immesso effettivamente nello stream soltanto quando verrà schedulata la write_deferred.
 */
int write_low(const char *buff, size_t len, session_state *session, object_state *the_object) {
    int ret;
    packed_work_struct *packed_work;

    printk("%s: Deferred work requested.\n", MODNAME);

    packed_work = kzalloc(sizeof(packed_work_struct), GFP_ATOMIC);
    if (packed_work == NULL) {
        printk("%s: Packed work_struct allocation failure\n", MODNAME);
        return -ENOMEM;
    }

    packed_work->len = len;
    packed_work->session = session;
    packed_work->minor = Minor;

    packed_work->data = kzalloc(len, GFP_ATOMIC);
    if (packed_work->data == NULL) {
        printk("%s: Packet work_struct data allocation failure\n", MODNAME);
        return -ENOMEM;
    }

    ret = copy_from_user((char *)packed_work->data, buff, len);
    if (ret != 0) {
        printk("%s: Packed work_struct data copy failure.\n", MODNAME);
    }

    printk("%s: Packed work_struct correctly allocated.\n", MODNAME);

    // Inizializza la work_struct nella struttura packed, con function pointer a write_deferred
    __INIT_WORK(&(packed_work->work), (void *)write_deferred, (unsigned long)&(packed_work->work));
    schedule_work(&packed_work->work);

    return len - ret;
}

/**
 * Effettua la scrittura in modalità deferred. Il numero di bytes scritti vengono notificati immediatamente
 * quindi non viene ritornato nulla in questa funzione, che non può fallire nel locking.
 */
void write_deferred(struct work_struct *deferred_work) {
    const char *buff;
    packed_work_struct *packed = container_of(deferred_work, packed_work_struct, work);
    int minor = packed->minor;
    session_state *session = packed->session;
    object_state *the_object = &objects[minor][LOW_PRIORITY];
    size_t len = packed->len;

    buff = packed->data;

    // printk("buff: %s\n", buff);s
    // loff_t *off = container_of(deferred_work, packed_work_struct, work)->off;

    printk("%s: kworker daemon awaken with PID=%d. Writing.\n", MODNAME, current->pid);
    write_on_stream(buff, len, session, the_object);
}