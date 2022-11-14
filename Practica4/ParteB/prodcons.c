#include <linux/module.h>
#include <asm-generic/errno.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0
#define NR_GPIO_LEDS 3

MODULE_DESCRIPTION("ModledsPi_gpiod Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

#define SUCCESS 0
#define DEVICE_NAME "prodcons "
#define CLASS_NAME "pr4parteb"
#define BUF_LEN 80
#define MAX_ITEMS_CBUF 4

struct kfifo cbuf;

static dev_t start;
static struct cdev *pileds;
static struct class *class = NULL;
static struct device *device = NULL;

static int Device_Open = 0;

static int prodcon_open(struct inode *inode, struct file *file)
{
    if (Device_Open)
        return -EBUSY;

    Device_Open++;

    // HACER LAS COSAS.

    /* Increment the module's reference counter */
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

static int prodcon_release(struct inode *inode, struct file *file)
{
    Device_Open--; /* We're now ready for our next caller */

    /*
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module.
     */
    module_put(THIS_MODULE);

    return 0;
}

ssize_t prodcons_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    char kbuf[MAX_CHARS_KBUF + 1];
    int nr_bytes = 0;
    int val;
    if ((*off) > 0)
        return 0;
    if (down_interruptible(&elementos))
        return -EINTR;
    /* Extraer el primer entero del buffer */
    kfifo_out(&cbuf, &val, sizeof(int));
    up(&huecos);
    nr_bytes = sprintf(kbuf, "%i\n", val);

    ///... copy_to_user()...
    return nr_bytes;
}

/*
 * Called when a process writes to dev file: echo "hi" > /dev/chardev
 */
static ssize_t
prodcon_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    char kbuf[MAX_CHARS_KBUF + 1];
    int val = 0;
    ..copy_from_user() + Convertir char *a entero y almacenarlo en val..if (down_interruptible(&huecos)) return -EINTR;
    /* Inserción en el buffer circular */
    kfifo_in(&cbuf, &val, sizeof(int));
    up(&elementos);
    return len;
}

static const struct file_operations fops = {
    .read = prodcon_read,
    .write = prodcon_write,
    .open = prodcon_open,
    .release = prodcon_release,
};

/**
 * Set up permissions for device files created by this driver
 *
 * The permission mode is returned using the mode parameter.
 *
 * The return value (unused here) could be used to indicate the directory
 * name under /dev where to create the device file. If used, it should be
 * a string whose memory must be allocated dynamically.
 **/
static char *cool_devnode(struct device *dev, umode_t *mode)
{
    if (!mode)
        return NULL;
    if (MAJOR(dev->devt) == MAJOR(start))
        *mode = 0666;
    return NULL;
}

static int __init modleds_init(void)
{
    int i, j;

    int major; /* Major number assigned to our device driver */
    int minor; /* Minor number assigned to the associated character device */
    int ret;

    if (kfifo_alloc(&cbuf, MAX_ITEMS_CBUF * sizeof(int), GFP_KERNEL))
        return -ENOMEM;
    sema_init(&elementos, 0);
    sema_init(&huecos, MAX_ITEMS_CBUF);

    /* Get available (major,minor) range */
    if ((ret = alloc_chrdev_region(&start, 0, 1, DEVICE_NAME)))
    {
        printk(KERN_INFO "Can't allocate chrdev_region()");
        return ret;
    }

    /* Create associated cdev */
    if ((pileds = cdev_alloc()) == NULL)
    {
        printk(KERN_INFO "cdev_alloc() failed ");
        ret = -ENOMEM;
        goto error_alloc;
    }

    cdev_init(pileds, &fops);

    if ((ret = cdev_add(pileds, start, 1)))
    {
        printk(KERN_INFO "cdev_add() failed ");
        goto error_add;
    }

    /* Create custom class */
    class = class_create(THIS_MODULE, CLASS_NAME);

    if (IS_ERR(class))
    {
        pr_err("class_create() failed \n");
        ret = PTR_ERR(class);
        goto error_class;
    }

    /* Establish function that will take care of setting up permissions for device file */
    class->devnode = cool_devnode;

    /* Creating device */
    device = device_create(class, NULL, start, NULL, DEVICE_NAME);

    if (IS_ERR(device))
    {
        pr_err("Device_create failed\n");
        ret = PTR_ERR(device);
        goto error_device;
    }

    major = MAJOR(start);
    minor = MINOR(start);

    return 0;

    // Cambiar orden.

error_device:
    class_destroy(class);
error_class:
    /* Destroy chardev */
    if (pileds)
    {
        cdev_del(pileds);
        pileds = NULL;
    }
error_add:
    /* Destroy partially initialized chardev */
    if (pileds)
        kobject_put(&pileds->kobj);
error_alloc:
    unregister_chrdev_region(start, 1);

    return ret;
}

static void __exit modleds_exit(void)
{
    int i = 0;

    /* Destroy the device and the class */
    if (device)
        device_destroy(class, device->devt);

    if (class)
        class_destroy(class);

    /* Destroy chardev */
    if (pileds)
        cdev_del(pileds);

    /*
     * Release major minor pair
     */
    unregister_chrdev_region(start, 1);
}

module_init(modleds_init);
module_exit(modleds_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modleds");