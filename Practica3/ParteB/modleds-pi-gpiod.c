#include <linux/module.h>
#include <asm-generic/errno.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>


#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0
#define NR_GPIO_LEDS  3

MODULE_DESCRIPTION("ModledsPi_gpiod Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

#define SUCCESS 0
#define DEVICE_NAME "leds"
#define CLASS_NAME "pileds"
#define BUF_LEN 80

static dev_t start;
static struct cdev* pileds;
static struct class* class = NULL;
static struct device* device = NULL;

static int Device_Open = 0;


/* Actual GPIOs used for controlling LEDs */
const int led_gpio[NR_GPIO_LEDS] = {25, 27, 4};

/* Array to hold gpio descriptors */
struct gpio_desc* gpio_descriptors[NR_GPIO_LEDS];

/* Set led state to that specified by mask */
static inline int set_pi_leds(unsigned int mask) {
  int i;
  for (i = 0; i < NR_GPIO_LEDS; i++)
    gpiod_set_value(gpio_descriptors[i], (mask >> i) & 0x1 );
  return 0;
}


static int led_open(struct inode *inode, struct file *file)
{
    if (Device_Open)
        return -EBUSY;

    Device_Open++;

    //HACER LAS COSAS.

    /* Increment the module's reference counter */
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

static int led_release(struct inode *inode, struct file *file)
{
    Device_Open--;      /* We're now ready for our next caller */

    /*
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module.
     */
    module_put(THIS_MODULE);

    return 0;
}

/*
 * Called when a process writes to dev file: echo "hi" > /dev/chardev
 */
static ssize_t
led_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{ 
    char kbuf[BUF_LEN];
    unsigned int mask;

    if((*off) > 0){
      return 0;
    }

    if(len > BUF_LEN-1){
      return -ENOSPC;
    }

    if(copy_from_user(kbuf, buff, len)){
      return -EFAULT;
    }

    kbuf[len] = '\0';

    if(sscanf(kbuf, "%x", &mask) != 1){
      return -EINVAL;
    }

    if(mask > 0x7){
      return -EINVAL;
    }

    printk(KERN_INFO "Modleds: mask:%x", mask);
    
    set_pi_leds(mask);

    *off+=len;

    return len;
}

static const struct file_operations fops = {
  .write = led_write,
  .open = led_open,
  .release = led_release,
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
  int err = 0;
  char gpio_str[10];

  int major;      /* Major number assigned to our device driver */
  int minor;      /* Minor number assigned to the associated character device */
  int ret;

    
  /* Get available (major,minor) range */
  if ((ret = alloc_chrdev_region (&start, 0, 1, DEVICE_NAME))) {
      printk(KERN_INFO "Can't allocate chrdev_region()");
      return ret;
  }

  /* Create associated cdev */
  if ((pileds = cdev_alloc()) == NULL) {
      printk(KERN_INFO "cdev_alloc() failed ");
      ret = -ENOMEM;
      goto error_alloc;
  }

  cdev_init(pileds, &fops);

  if ((ret = cdev_add(pileds, start, 1))) {
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
  class->devnode = cool_devnode;

  /* Creating device */
  device = device_create(class, NULL, start, NULL, DEVICE_NAME);

  if (IS_ERR(device)) {
      pr_err("Device_create failed\n");
      ret = PTR_ERR(device);
      goto error_device;
  }

  major = MAJOR(start);
  minor = MINOR(start);

  for (i = 0; i < NR_GPIO_LEDS; i++) {
    /* Build string ID */
    sprintf(gpio_str, "led_%d", i);
    /* Request GPIO */
    if ((err = gpio_request(led_gpio[i], gpio_str))) {
      pr_err("Failed GPIO[%d] %d request\n", i, led_gpio[i]);
      goto err_handle;
    }

    /* Transforming into descriptor */
    if (!(gpio_descriptors[i] = gpio_to_desc(led_gpio[i]))) {
      pr_err("GPIO[%d] %d is not valid\n", i, led_gpio[i]);
      err = -EINVAL;
      goto err_handle;
    }

    gpiod_direction_output(gpio_descriptors[i], 0);
  }

  //set_pi_leds(ALL_LEDS_ON);
  return 0;


err_handle:
  for (j = 0; j < i; j++)
    gpiod_put(gpio_descriptors[j]);
  return err;
error_device:
    class_destroy(class);
error_class:
    /* Destroy chardev */
    if (pileds) {
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

static void __exit modleds_exit(void) {
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

  set_pi_leds(ALL_LEDS_OFF);

  for (i = 0; i < NR_GPIO_LEDS; i++)
    gpiod_put(gpio_descriptors[i]);
}




module_init(modleds_init);
module_exit(modleds_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modleds");
