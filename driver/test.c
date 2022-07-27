
/*
 *  baseline char device driver with limitation on minor numbers - no actual operations
 */

#define EXPORT_SYMTAB
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pid.h> /* For pid types */
#include <linux/sched.h>
#include <linux/tty.h>     /* For the tty declarations */
#include <linux/version.h> /* For LINUX_VERSION_CODE */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia");

#define MODNAME "CHAR DEV"

/*
 * Prototipi delle funzioni del driver
 */
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

#define DEVICE_NAME "my-new-dev" /* Device file name in /dev/ - not mandatory  */
#define OBJECT_MAX_SIZE (4096)   // just one page

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
 *
 */
typedef struct _object_state {
    struct mutex object_busy;
    struct mutex operation_synchronizer;
    int valid_bytes;
    char *stream_content;  // Il nodo di I/O è un buffer di memoria, che viene puntato tramite questo campo
} object_state;

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

    if (!mutex_trylock(&(objects[minor].object_busy))) {
        return -EBUSY;
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
    mutex_unlock(&(objects[minor].object_busy));
    printk("%s: device file closed\n", MODNAME);
    return 0;
}

/**
 * Funzione eseguita effettivamente quando andiamo a chiamare una write() a livello applicativo, una sys_write()
 * @param filp puntatore alla struttura dati che rappresenta la sessione
 * @param buff puntatore all'area di memoria applicativa passato dalla syscall
 * @param len taglia della write
 * @param off offset all'area di memoria filp su cui lavoriamo.
 */
static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off) {
    int minor = get_minor(filp);
    int ret;
    object_state *the_object;

    the_object = objects + minor;
    printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n", MODNAME, get_major(filp), get_minor(filp));

    // need to lock in any case
    mutex_lock(&(the_object->operation_synchronizer));
    if (*off >= OBJECT_MAX_SIZE) {  // offset too large
        mutex_unlock(&(the_object->operation_synchronizer));
        return -ENOSPC;  // no space left on device
    }
    if (*off > the_object->valid_bytes) {  // offset bwyond the current stream size
        mutex_unlock(&(the_object->operation_synchronizer));
        return -ENOSR;  // out of stream resources
    }
    if ((OBJECT_MAX_SIZE - *off) < len) len = OBJECT_MAX_SIZE - *off;
    ret = copy_from_user(&(the_object->stream_content[*off]), buff, len);

    *off += (len - ret);
    the_object->valid_bytes = *off;
    mutex_unlock(&(the_object->operation_synchronizer));

    return len - ret;
}

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {
    int minor = get_minor(filp);
    int ret;
    object_state *the_object;

    the_object = objects + minor;
    printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n", MODNAME, get_major(filp), get_minor(filp));

    // need to lock in any case
    mutex_lock(&(the_object->operation_synchronizer));
    if (*off > the_object->valid_bytes) {
        mutex_unlock(&(the_object->operation_synchronizer));
        return 0;
    }
    if ((the_object->valid_bytes - *off) < len) len = the_object->valid_bytes - *off;
    ret = copy_to_user(buff, &(the_object->stream_content[*off]), len);

    *off += (len - ret);
    mutex_unlock(&(the_object->operation_synchronizer));

    return len - ret;
    printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n", MODNAME, get_major(filp), get_minor(filp));

    return 0;
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
    .write = dev_write,
    .read = dev_read,
    .open = dev_open,
    .release = dev_release

};

/*
 *  Inizializza il modulo, scrivendo sul buffer del kernel il Major Number assegnato al Driver.
 */
int init_module(void) {
    int i;

    // Inizializzo lo stato di ogni device.
    for (i = 0; i < MINORS; i++) {
        mutex_init(&(objects[i].object_busy));
        mutex_init(&(objects[i].operation_synchronizer));
        objects[i].valid_bytes = 0;
        objects[i].stream_content = NULL;
        // Allocazione di una pagina (4KB) tramite Buddy Allocator
        objects[i].stream_content = (char *)__get_free_pages(GFP_KERNEL, 1);
        printk(KERN_INFO "initialized buffer for minor %d: %s", i, objects[i].stream_content);

        if (objects[i].stream_content == NULL) goto revert_allocation;
    }

    // Registro il device, con major 0 per allocazione dinamica. count=128 perchè voglio gestire massimo 128 devices
    Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);

    if (Major < 0) {
        printk("%s: registering device failed\n", MODNAME);
        return Major;
    }

    printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n", MODNAME, Major);

    return 0;

revert_allocation:
    printk(KERN_INFO "revert allocation\n");
    for (; i >= 0; i--) {
        free_page((unsigned long)objects[i].stream_content);
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
