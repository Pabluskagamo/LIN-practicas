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

static struct music_step melodic_line[] = {
		{C4, 4}, {E4, 4}, {G4, 4}, {C5, 4}, 
		{0, 2}, {C5, 4}, {G4, 4}, {E4, 4}, 
		{C4, 4}, {0, 0} /* Terminator */
};

static const int beat = 120; /* 120 quarter notes per minute */
static struct music_step *next = NULL;

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
		pwm_disable(pwm_device);
	}

}


/* Function invoked when timer expires (fires) */
static void fire_timer(struct timer_list *timer)
{
	printk(KERN_INFO "Estoy en fire timer");

	if(next == NULL){
		next = melodic_line;
	}else{
		/* Initialize work structure (with function) */
		INIT_WORK(&my_work, my_wq_function);
		/* Enqueue work */
		schedule_work(&my_work);
	}
}

static int __init pwm_module_init(void)
{
	int err = 0;
	unsigned char gpio_out_ok = 0;

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

	/* Create timer */
    timer_setup(&my_timer, fire_timer, 0);

	my_timer.expires = jiffies; /* Activate it one second from now */
    /* Activate the timer for the first time */
    add_timer(&my_timer);

	printk(KERN_INFO "Modulo Buzzer cargado");

	//schedule_work(&my_work);

	return 0;

err_handle:
  if (gpio_out_ok)
    gpiod_put(desc_button);

  return err;
}

static void __exit pwm_module_exit(void)
{
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
