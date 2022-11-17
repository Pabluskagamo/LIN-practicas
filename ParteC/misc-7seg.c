#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm-generic/errno.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/semaphore.h>

MODULE_DESCRIPTION("Misc Display7s Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

/* Bits associated with each segment */
#define DS_0 0xFC
#define DS_1 0x60
#define DS_2 0xDA
#define DS_3 0xF2
#define DS_4 0x66
#define DS_5 0xB6
#define DS_6 0xBE
#define DS_7 0xE0
#define DS_8 0xFE
#define DS_9 0xF6
#define DS_A 0xEE
#define DS_B 0x3E
#define DS_C 0x9C
#define DS_D 0x7A
#define DS_E 0x9E
#define DS_F 0x8E
#define SEGMENT_COUNT 8
#define BUF_LEN 80
#define SUCCESS 0

struct semaphore abiertos, escribir;

/* Indices of GPIOs used by this module */
enum
{
	SDI_IDX = 0,
	RCLK_IDX,
	SRCLK_IDX,
	NR_GPIO_DISPLAY
};

static int Device_Open = 0;

/* Pin numbers */
const int display_gpio[NR_GPIO_DISPLAY] = {18, 23, 24};

/* Array to hold GPIO descriptors */
struct gpio_desc *gpio_descriptors[NR_GPIO_DISPLAY];

const char *display_gpio_str[NR_GPIO_DISPLAY] = {"sdi", "rclk", "srclk"};

/* Sequence of segments used by the character device driver */
const int sequence[] = {DS_0, DS_1, DS_2, DS_3, DS_4, DS_5, DS_6, DS_7, DS_8, DS_9, DS_A, DS_B, DS_C, DS_D, DS_E, DS_F};

#define DEVICE_NAME "display7s" /* Device name */

/*
 *  Prototypes
 */
static ssize_t display7s_write(struct file *, const char *, size_t, loff_t *);
static int display7s_open(struct inode *inode, struct file *file);
static int display7s_release(struct inode *inode, struct file *file);

/* Simple initialization of file_operations interface with a single operation */
static struct file_operations fops = {
	.open = display7s_open,
	.release = display7s_release,
	.write = display7s_write,
};

static struct miscdevice display7s_misc = {
	.minor = MISC_DYNAMIC_MINOR, /* kernel dynamically assigns a free minor# */
	.name = DEVICE_NAME,		 /* when misc_register() is invoked, the kernel
								  * will auto-create device file;
								  * also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
	.mode = 0666,				 /* ... dev node perms set as specified here */
	.fops = &fops,				 /* connect to this driver's 'functionality' */
};


/* Update the 7-segment display with the configuration specified by the data parameter */
static void update_7sdisplay(unsigned char data)
{
	int i = 0;
	int value = 0;

	for (i = 0; i < SEGMENT_COUNT; i++)
	{
		/* Explore current bit (from most significant to least significant) */
		if (0x80 & (data << i))
			value = 1;
		else
			value = 0;

		/* Set value of serial input */
		gpiod_set_value(gpio_descriptors[SDI_IDX], value);
		/* Generate clock cycle in shift register */
		gpiod_set_value(gpio_descriptors[SRCLK_IDX], 1);
		mdelay(1);
		gpiod_set_value(gpio_descriptors[SRCLK_IDX], 0);
	}

	/* Generate clock cycle in output register to update 7-seg display */
	gpiod_set_value(gpio_descriptors[RCLK_IDX], 1);
	mdelay(1);
	gpiod_set_value(gpio_descriptors[RCLK_IDX], 0);
}

static int 
display7s_open(struct inode *inode, struct file *file){

	if (down_interruptible(&abiertos))
        return -EINTR;

	if(Device_Open){
		up(&abiertos);
		return -EBUSY;
	}

	Device_Open++;

	up(&abiertos);

    /* Increment the module's reference counter */
    try_module_get(THIS_MODULE);

    return SUCCESS;
}


static int 
display7s_release(struct inode *inode, struct file *file){

	if (down_interruptible(&abiertos))
        return -EINTR;

	Device_Open--; /* We're now ready for our next caller */

	up(&abiertos);
    /*
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module.
     */
    module_put(THIS_MODULE);

    return 0;
}

static ssize_t
display7s_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	char kbuf[BUF_LEN];
    unsigned int n;

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

	if(sscanf(kbuf, "%x", &n) != 1){
      return -EINVAL;
    }

	if(n > -1 && n < 16){
		return -EINVAL;
	}

	if (down_interruptible(&escribir))
        return -EINTR;

	update_7sdisplay(sequence[n]);

	up(&escribir);

	*off+=len;

	return len;
}

static int __init display7s_misc_init(void)
{
	int i, j;
	int err = 0;
	struct device *device;

	for (i = 0; i < NR_GPIO_DISPLAY; i++)
	{

		/* Request the GPIO */
		if ((err = gpio_request(display_gpio[i], display_gpio_str[i])))
		{
			pr_err("Failed GPIO[%d] %d request\n", i, display_gpio[i]);
			goto err_handle;
		}

		/* Transform number into descriptor */
		if (!(gpio_descriptors[i] = gpio_to_desc(display_gpio[i])))
		{
			pr_err("GPIO[%d] %d is not valid\n", i, display_gpio[i]);
			err = -EINVAL;
			goto err_handle;
		}

		/* Configure as an output pin */
		gpiod_direction_output(gpio_descriptors[i], 0);
	}

	/* Set everything as LOW */
	for (i = 0; i < NR_GPIO_DISPLAY; i++)
		gpiod_set_value(gpio_descriptors[i], 0);

	/* Register misc device that exposes 7-segment display */
	err = misc_register(&display7s_misc);

	if (err)
	{
		pr_err("Couldn't register misc device\n");
		goto err_handle;
	}

	device = display7s_misc.this_device;

	dev_info(device, "Display7s driver registered succesfully. To talk to\n");
	dev_info(device, "the driver try to cat and echo to /dev/%s.\n", DEVICE_NAME);
	dev_info(device, "Remove the module when done.\n");

	sema_init(&abiertos, 1);
	sema_init(&escribir, 1);

	return 0;
err_handle:
	for (j = 0; j < i; j++)
		gpiod_put(gpio_descriptors[j]);
	return err;
}

static void __exit display7s_misc_exit(void)
{
	int i = 0;

	/* Unregister character device */
	misc_deregister(&display7s_misc);

	/* Clear display */
	update_7sdisplay(0);

	/* Free up pins */
	for (i = 0; i < NR_GPIO_DISPLAY; i++)
		gpiod_put(gpio_descriptors[i]);

	pr_info("Display7s driver deregistered. Bye\n");
}

module_init(display7s_misc_init);
module_exit(display7s_misc_exit);
