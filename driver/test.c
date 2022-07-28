#define EXPORT_SYMTAB

#include "utils/structures.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Danilo Dell'Orco");

#define MODNAME "MULTI-FLOW DEV"
#define DEVICE_NAME "test-dev" /* Device file name in /dev/ - not mandatory  */

/*
 * Prototipi delle funzioni del driver
 */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

#define OBJECT_MAX_SIZE (4096)  // just one page

static int Major; /* Major number assigned to broadcast device driver */

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
 * Dobbiamo gestire 128 dispositivi di I/O, quindi 128 minor numbers differenti.
 * Definiamo quindi 128 differenti strutture object_state mantenute nell'array objects
 **/
#define MINORS 3
object_state objects[MINORS];

//
//
//
//
//
//
/* the actual driver */

/*
 * Invocata dal VFS quando apriamo il nodo associato al driver. Cerca di prendere il lock su un mutex,
 * quindi si può avere una sola sessione per volta verso l'oggetto.
 */
static int dev_open(struct inode *inode, struct file *file) {
    int minor;
    minor = get_minor(file);

    if (minor >= MINORS) {
        return -ENODEV;
    }

    printk("%s: device file successfully opened for object with minor %d\n", MODNAME, minor);
    // device opened by a default nop
    return 0;
}

/**
 * Invocata dal VFS quando si chiude il file, quindi la open è già stata effettuata correttamente
 */
static int dev_release(struct inode *inode, struct file *file) {
    int minor;
    minor = get_minor(file);
    printk("%s: device file closed\n", MODNAME);
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
    int minor = get_minor(filp);
    int ret;
    object_state *the_object;

    stream_block *block = kzalloc(sizeof(stream_block), GFP_ATOMIC);
    char *block_buff = kzalloc(len, GFP_ATOMIC);

    the_object = objects + minor;
    printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n", MODNAME, get_major(filp), get_minor(filp));

    // need to lock in any case
    mutex_lock(&(the_object->operation_synchronizer));

    ret = copy_from_user(block_buff, buff, len);

    *off += (len - ret);
    the_object->valid_bytes = *off;
    block->next = NULL;
    block->stream_content = NULL;

    the_object->tail->next = block;
    the_object->tail->stream_content = block_buff;

    mutex_unlock(&(the_object->operation_synchronizer));

    ret = clear_user(buff, len);
    printk("%s: write clear_user %d\n", MODNAME, ret);

    return len - ret;
}

/**
 * Funzione eseguita effettivamente quando andiamo a chiamare una read() a livello applicativo
 * @param filp puntatore alla struttura dati che rappresenta la sessione
 * @param buff puntatore all'area di memoria applicativa passato dalla syscall
 * @param len taglia della write
 * @param off offset all'area di memoria filp su cui lavoriamo.
 */
static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {
    int minor = get_minor(filp);
    int ret;
    int block_size;
    object_state *the_object;
    stream_block *current_block;
    stream_block *completed_block;

    ret = clear_user(buff, len);
    printk("%s: read clear_user %d\n", MODNAME, ret);

    the_object = objects + minor;
    printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n", MODNAME, get_major(filp), get_minor(filp));
    printk("%s: offset: %d\n", MODNAME, the_object->head->read_offset);

    // need to lock in any case
    mutex_lock(&(the_object->operation_synchronizer));
    // if (the_object->read_offset > the_object->valid_bytes) {
    //     printk("%s: read lock error (1)", MODNAME);
    //     mutex_unlock(&(the_object->operation_synchronizer));
    //     return 0;
    // }

    block_size = strlen(current_block->stream_content);
    current_block = the_object->head;
    int to_read = len;
    // printk("%s: valid_bytes: %d, len: %lu\n", MODNAME, the_object->valid_bytes, len);

    while (to_read != 0) {
        if (block_size - current_block->read_offset < to_read) {
            // TODO devo proseguire la lettura nel blocco successivo
            printk("%s: cross block read", MODNAME);
            len = block_size - current_block->read_offset;
            to_read -= len;
            ret += copy_to_user(&buff[ret], &(current_block->stream_content[current_block->read_offset]), len);
            ret += len;

            // Tutti i dati sono stati letti dal blocco, quindi posso rimuoverlo e deallocare la memoria
            completed_block = current_block;
            current_block = current_block->next;
            the_object->head = current_block;
            block_size = strlen(current_block->stream_content);

            kfree(completed_block->stream_content);
            kfree(completed_block);

        } else {
            ret = copy_to_user(buff, &(current_block->stream_content[current_block->read_offset]), len);
            printk("%s: copy_to_user ret: %d", MODNAME, ret);
            current_block->read_offset += (len - ret);
            mutex_unlock(&(the_object->operation_synchronizer));
        }
    }

    return len - ret;
}

/**
 * Permette di controllare i parametri della sessione di I/O
 */
static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param) {
    int minor = get_minor(filp);
    object_state *the_object;

    the_object = objects + minor;
    printk("%s: somebody called an ioctl on dev with [major,minor] number [%d,%d] and command %u \n", MODNAME, get_major(filp), get_minor(filp), command);

    // do here whathever you would like to control the state of the device
    return 0;
}

/*
 * Assegnazione della struttura driver, specificando i puntatori alle varie file operations implementate
 */

static struct file_operations fops = {
    .owner = THIS_MODULE,  // do not forget this
    .write = dev_write,
    .read = dev_read,
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = dev_ioctl};

/*
 *  Inizializza il modulo, scrivendo sul buffer del kernel il Major Number assegnato al Driver.
 */
int init_module(void) {
    int i;

    // Inizializzo lo stato di ogni device.
    for (i = 0; i < MINORS; i++) {
        mutex_init(&(objects[i].operation_synchronizer));
        objects[i].head->next = NULL;
        objects[i].head->stream_content = NULL;
        objects[i].head->read_offset = 0;
        objects[i].valid_bytes = 0;

        // Allocazione per il primo blocco di stream
        objects[i].head = kzalloc(sizeof(stream_block), GFP_KERNEL);
        objects[i].tail = objects[i].head;

        if (objects[i].head == NULL) goto revert_allocation;
    }

    // Registro il device, con major 0 per allocazione dinamica. count=128 perchè voglio gestire massimo 128 devices
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
        kfree(objects[i].head);
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
