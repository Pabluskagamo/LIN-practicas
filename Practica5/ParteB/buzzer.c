#include <linux/module.h>
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h> /* For fg_console */
#include <linux/kd.h>  /* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/pwm.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>

MODULE_DESCRIPTION("Test-buzzer Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

struct timer_list my_timer;

#define MANUAL_DEBOUNCE

/* Frequency of selected notes in centihertz */
#define C4 26163
#define D4 29366
#define E4 32963
#define F4 34923
#define G4 39200
#define C5 52325


#define SUCCESS 0
#define DEVICE_NAME "buzzer"
#define CLASS_NAME "pibuzzer"
#define BUF_LEN 2048 //Momentaneamente.

static dev_t start;
static struct cdev* buzzer;
static struct class* class = NULL;
static struct device* device = NULL;

static int Device_Open = 0;


#define PWM_DEVICE_NAME "pwmchip0"
#define GPIO_BUTTON 22

struct gpio_desc* desc_button = NULL;
static int gpio_button_irqn = -1;

struct pwm_device *pwm_device = NULL;
struct pwm_state pwm_state;

/* Work descriptor */
struct work_struct my_work;

/* Structure to represent a note or rest in a melodic line  */
struct music_step
{
	unsigned int freq : 24; /* Frequency in centihertz */
	unsigned int len : 8;	/* Duration of the note */
};

struct music_step *melody;

static struct music_step default_melody[] = {
		{C4, 4}, {E4, 4}, {G4, 4}, {C5, 4}, 
		{0, 2}, {C5, 4}, {G4, 4}, {E4, 4}, 
		{C4, 4}, {0, 0} /* Terminator */
};

static int beat = 120; /* 120 quarter notes per minute */
static struct music_step *next = NULL;


static int buzzer_open(struct inode *inode, struct file *file)
{
    if (Device_Open)
        return -EBUSY;

    Device_Open++;

    //HACER LAS COSAS.

    /* Increment the module's reference counter */
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

static int buzzer_release(struct inode *inode, struct file *file)
{
    Device_Open--;      /* We're now ready for our next caller */

    /*
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module.
     */
    module_put(THIS_MODULE);

    return 0;
}

static ssize_t
buzzer_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{ 
    char kbuf[BUF_LEN];
	char *token;
	char* puntero;
	int retval = 0;
	int readBeat;
	struct music_step *aux;

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


	if(sscanf(kbuf, "music %s", &kbuf) == 1){
		puntero = kbuf;
		token = strsep(&puntero, ",");

		aux = melody;
		while(token != NULL){
			unsigned int freq, dur;
					
			if(sscanf(token, "%u:%x", &freq, &dur) != 2){
				retval = -EINVAL;
				goto out_error;
			}
					
			aux->freq = freq;
			aux->len = dur;
			aux++;

			token = strsep(&puntero, ",");
		}
		aux->freq = 0;
		aux->len = 0;
	}else if(sscanf(kbuf, "beat %i", &readBeat) == 1){
		beat = readBeat;
	}else{
		return -EINVAL;
	}
    
    *off+=len;

    return len;
out_error:
	//kfree(message);
	return retval;	
}

ssize_t buzzer_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    char kbuf[BUF_LEN + 1];
    int nr_bytes = 0;

    if ((*off) > 0)
        return 0;
    
    nr_bytes = sprintf(kbuf, "beat=%i\n", beat);

    if(copy_to_user(buf, kbuf, nr_bytes))
        return -EINVAL;

    (*off) += len;

    return nr_bytes;
}

static const struct file_operations fops = {
  .write = buzzer_write,
  .open = buzzer_open,
  .release = buzzer_release,
  .read = buzzer_read
};

static char *cool_devnode(struct device *dev, umode_t *mode)
{
    if (!mode)
        return NULL;
    if (MAJOR(dev->devt) == MAJOR(start))
        *mode = 0666;
    return NULL;
}

/* Interrupt handler for button **/
static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
#ifdef MANUAL_DEBOUNCE
  static unsigned long last_interrupt = 0;
  unsigned long diff = jiffies - last_interrupt;
  if (diff < 20)
    return IRQ_HANDLED;

  last_interrupt = jiffies;
#endif
	printk(KERN_INFO "Estoy en IRQ");
	mod_timer(&my_timer, jiffies);  
  return IRQ_HANDLED;
}


/* Transform frequency in centiHZ into period in nanoseconds */
static inline unsigned int freq_to_period_ns(unsigned int frequency)
{
	if (frequency == 0)
		return 0;
	else
		return DIV_ROUND_CLOSEST_ULL(100000000000UL, frequency);
}

/* Check if the current step is and end marker */
static inline int is_end_marker(struct music_step *step)
{
	return (step->freq == 0 && step->len == 0);
}

/**
 *  Transform note length into ms,
 * taking the beat of a quarter note as reference
 */
static inline int calculate_delay_ms(unsigned int note_len, unsigned int qnote_ref)
{
	unsigned char duration = (note_len & 0x7f);
	unsigned char triplet = (note_len & 0x80);
	unsigned char i = 0;
	unsigned char current_duration;
	int total = 0;

	/* Calculate the total duration of the note
	 * as the summation of the figures that make
	 * up this note (bits 0-6)
	 */
	while (duration) {
		current_duration = (duration) & (1 << i);

		if (current_duration) {
			/* Scale note accordingly */
			if (triplet)
				current_duration = (current_duration * 3) / 2;
			/*
			 * 24000/qnote_ref denote number of ms associated
			 * with a whole note (redonda)
			 */
			total += (240000) / (qnote_ref * current_duration);
			/* Clear bit */
			duration &= ~(1 << i);
		}
		i++;
	}
	return total;
}


/* Work's handler function */
static void my_wq_function(struct work_struct *work)
{
	pwm_init_state(pwm_device, &pwm_state);

	/* Play notes sequentially until end marker is found */
	if(!is_end_marker(next)) {
		printk(KERN_INFO "Estoy en la workqueue %i", next->freq);
		/* Obtain period from frequency */
		pwm_state.period = freq_to_period_ns(next->freq);

		/**
		 * Disable temporarily to allow repeating the same consecutive
		 * notes in the melodic line
		 **/
		pwm_disable(pwm_device);

		/* If period==0, its a rest (silent note) */
		if (pwm_state.period > 0) {
			/* Set duty cycle to 70 to maintain the same timbre */
			pwm_set_relative_duty_cycle(&pwm_state, 70, 100);
			pwm_state.enabled = true;
			/* Apply state */
			pwm_apply_state(pwm_device, &pwm_state);
		} else {
			/* Disable for rest */
			pwm_disable(pwm_device);
		}

		/* Wait for duration of the note or reset */
		mod_timer(&my_timer, jiffies + msecs_to_jiffies(calculate_delay_ms(next->len, beat)));
		next++;
	}else{
		next = melody;
		pwm_disable(pwm_device);
	}

}


/* Function invoked when timer expires (fires) */
static void fire_timer(struct timer_list *timer)
{
	printk(KERN_INFO "Estoy en fire timer");

	if(next == NULL){
		next = melody;
	}else{
		/* Initialize work structure (with function) */
		INIT_WORK(&my_work, my_wq_function);
		/* Enqueue work */
		schedule_work(&my_work);
	}
}

static void init_defaultMelody(void){
	struct music_step *aux;
	struct music_step *nextMel = melody;

	for(aux = default_melody; !is_end_marker(aux); aux++){
		nextMel->freq = aux->freq;
		nextMel->len = aux->len;
		nextMel++;
	}
	nextMel->freq = 0;
	nextMel->len = 0;

}

static int __init pwm_module_init(void)
{
	int err = 0;
	unsigned char gpio_out_ok = 0;
	int major;      /* Major number assigned to our device driver */
  	int minor;      /* Minor number assigned to the associated character device */


	/* Create timer */
    timer_setup(&my_timer, fire_timer, 0);

	my_timer.expires = jiffies; /* Activate it one second from now */

	/* Activate the timer for the first time */
    add_timer(&my_timer);

	/* Requesting Button's GPIO */
	if ((err = gpio_request(GPIO_BUTTON, "button"))) {
		pr_err("ERROR: GPIO %d request\n", GPIO_BUTTON);
		goto err_handle;
	}

	/* Configure Button */
	if (!(desc_button = gpio_to_desc(GPIO_BUTTON))) {
		pr_err("GPIO %d is not valid\n", GPIO_BUTTON);
		err = -EINVAL;
		goto err_handle;
	}

	gpio_out_ok = 1;

	//configure the BUTTON GPIO as input
	gpiod_direction_input(desc_button);

	/*
	** The lines below are commented because gpiod_set_debounce is not supported
	** in the Raspberry pi. Debounce is handled manually in this driver.
	*/
	#ifndef MANUAL_DEBOUNCE
	//Debounce the button with a delay of 200ms
	if (gpiod_set_debounce(desc_button, 200) < 0) {
		pr_err("ERROR: gpio_set_debounce - %d\n", GPIO_BUTTON);
		goto err_handle;
	}
	#endif

	//Get the IRQ number for our GPIO
	gpio_button_irqn = gpiod_to_irq(desc_button);
	pr_info("IRQ Number = %d\n", gpio_button_irqn);

	if (request_irq(gpio_button_irqn,             //IRQ number
					gpio_irq_handler,   //IRQ handler
					IRQF_TRIGGER_RISING,        //Handler will be called in raising edge
					"button_leds",               //used to identify the device name using this IRQ
					NULL)) {                    //device id for shared IRQ
		pr_err("my_device: cannot register IRQ ");
		goto err_handle;
	}
	/* Request utilization of PWM0 device */
	pwm_device = pwm_request(0, PWM_DEVICE_NAME);

	if (IS_ERR(pwm_device))
		return PTR_ERR(pwm_device);

	melody = vmalloc(PAGE_SIZE);

	if(!melody){
		goto err_handle;
	}else{
		init_defaultMelody();
	}


	/* Get available (major,minor) range */
	if ((err = alloc_chrdev_region (&start, 0, 1, DEVICE_NAME))) {
		printk(KERN_INFO "Can't allocate chrdev_region()");
		return err;
	}

	/* Create associated cdev */
	if ((buzzer = cdev_alloc()) == NULL) {
		printk(KERN_INFO "cdev_alloc() failed ");
		err = -ENOMEM;
		goto error_alloc;
	}

	cdev_init(buzzer, &fops);

	if ((err = cdev_add(buzzer, start, 1))) {
		printk(KERN_INFO "cdev_add() failed ");
		goto error_add;
	}

	/* Create custom class */
	class = class_create(THIS_MODULE, CLASS_NAME);

	if (IS_ERR(class)) {
		pr_err("class_create() failed \n");
		err = PTR_ERR(class);
		goto error_class;
	}

	/* Establish function that will take care of setting up permissions for device file */
	class->devnode = cool_devnode;

	/* Creating device */
	device = device_create(class, NULL, start, NULL, DEVICE_NAME);

	if (IS_ERR(device)) {
		pr_err("Device_create failed\n");
		err = PTR_ERR(device);
		goto error_device;
	}

	major = MAJOR(start);
	minor = MINOR(start);

	printk(KERN_INFO "Modulo Buzzer cargado");

	return 0;

error_device:
    class_destroy(class);

error_class:
    /* Destroy chardev */
    if (buzzer) {
        cdev_del(buzzer);
        buzzer = NULL;
    }
error_add:
    /* Destroy partially initialized chardev */
    if (buzzer)
        kobject_put(&buzzer->kobj);
error_alloc:
    unregister_chrdev_region(start, 1);
err_handle:
  if (gpio_out_ok)
    gpiod_put(desc_button);

  return err;
}

static void __exit pwm_module_exit(void)
{
	/* Destroy the device and the class */
	if (device)
		device_destroy(class, device->devt);

	if (class)
		class_destroy(class);

	/* Destroy chardev */
	if (buzzer)
		cdev_del(buzzer);

	/*
	* Release major minor pair
	*/
	unregister_chrdev_region(start, 1);

	free_irq(gpio_button_irqn, NULL);
	/* Wait until defferred work has finished */
	flush_work(&my_work);

	gpiod_put(desc_button);

	/* Release PWM device */
	pwm_free(pwm_device);
}

module_init(pwm_module_init);
module_exit(pwm_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PWM test");
