#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>



MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Clipboard-Dev Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");

#define BUFFER_LENGTH       PAGE_SIZE
#define DEVICE_NAME "clipboard" /* Dev name as it appears in /proc/devices   */
#define CLASS_NAME "clip"


/*
 * Global variables are declared as static, so are global within the file.
 */

static dev_t start;
static struct cdev* chardev = NULL;
static struct class* class = NULL;
static struct device* device = NULL;
static char *clipboard;  // Space for the "clipboard"

static ssize_t clipboard_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
  int available_space = BUFFER_LENGTH - 1;

  if ((*off) > 0) /* The application can write in this entry just once !! */
    return 0;

  if (len > available_space) {
    printk(KERN_INFO "clipboard: not enough space!!\n");
    return -ENOSPC;
  }

  /* Transfer data from user to kernel space */
  if (copy_from_user( &clipboard[0], buf, len ))
    return -EFAULT;

  clipboard[len] = '\0'; /* Add the `\0' */
  *off += len;          /* Update the file position indicator */

  return len;
}

static ssize_t clipboard_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

  int nr_bytes;

  if ((*off) > 0) /* Tell the application that there is nothing left to read */
    return 0;

  nr_bytes = strlen(clipboard);

  if (len < nr_bytes)
    return -ENOSPC;

  /* Transfer data from the kernel to userspace */
  if (copy_to_user(buf, clipboard, nr_bytes))
    return -EINVAL;

  (*off) += len; /* Update the file position indicator */

  return nr_bytes;
}

static struct file_operations fops = {
  .read = clipboard_read,
  .write = clipboard_write,
};


static char *custom_devnode(struct device *dev, umode_t *mode)
{
  if (!mode)
    return NULL;
  if (MAJOR(dev->devt) == MAJOR(start))
    *mode = 0666;
  return NULL;
}

int init_clipboard_module( void )
{
  int major;    /* Major number assigned to our device driver */
  int minor;    /* Minor number assigned to the associated character device */
  int ret;

  clipboard = (char *)vmalloc( BUFFER_LENGTH );

  if (!clipboard)
    return  -ENOMEM;

  memset( clipboard, 0, BUFFER_LENGTH );

  /* Get available (major,minor) range */
  if ((ret = alloc_chrdev_region (&start, 0, 1, DEVICE_NAME))) {
    printk(KERN_INFO "Can't allocate chrdev_region()");
    goto error_alloc_region;
  }

  /* Create associated cdev */
  if ((chardev = cdev_alloc()) == NULL) {
    printk(KERN_INFO "cdev_alloc() failed ");
    ret = -ENOMEM;
    goto error_alloc;
  }

  cdev_init(chardev, &fops);

  if ((ret = cdev_add(chardev, start, 1))) {
    printk(KERN_INFO "cdev_add() failed ");
    goto error_add;
  }

  /* Create custom class */
  class = class_create(THIS_MODULE, CLASS_NAME);

  if (IS_ERR(class)) {
    pr_err("class_create() failed \n");
    ret = PTR_ERR(class);
    goto error_class;
  }

  /* Establish function that will take care of setting up permissions for device file */
  class->devnode = custom_devnode;

  /* Creating device */
  device = device_create(class, NULL, start, NULL, DEVICE_NAME);

  if (IS_ERR(device)) {
    pr_err("Device_create failed\n");
    ret = PTR_ERR(device);
    goto error_device;
  }

  major = MAJOR(start);
  minor = MINOR(start);

  printk(KERN_INFO "I was assigned major number %d. To talk to\n", major);
  printk(KERN_INFO "the driver try to cat and echo to /dev/%s.\n", DEVICE_NAME);
  printk(KERN_INFO "Remove the module when done.\n");

  printk(KERN_INFO "Clipboard-dev: Module loaded.\n");

  return 0;

error_device:
  class_destroy(class);
error_class:
  /* Destroy chardev */
  if (chardev) {
    cdev_del(chardev);
    chardev = NULL;
  }
error_add:
  /* Destroy partially initialized chardev */
  if (chardev)
    kobject_put(&chardev->kobj);
error_alloc:
  unregister_chrdev_region(start, 1);
error_alloc_region:
  vfree(clipboard);

  return ret;
}


void exit_clipboard_module( void )
{
  if (device)
    device_destroy(class, device->devt);

  if (class)
    class_destroy(class);

  /* Destroy chardev */
  if (chardev)
    cdev_del(chardev);

  /*
   * Release major minor pair
   */
  unregister_chrdev_region(start, 1);

  vfree(clipboard);

  printk(KERN_INFO "Clipboard-dev: Module unloaded.\n");
}


module_init( init_clipboard_module );
module_exit( exit_clipboard_module );
