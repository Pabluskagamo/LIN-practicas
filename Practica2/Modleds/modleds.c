#include <linux/module.h> 
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0
#define	MAX_K	128

//Entradas en /proc
static struct proc_dir_entry *proc_entry;

struct tty_driver* kbd_driver= NULL;

/* Get driver handler */
struct tty_driver* get_kbd_driver_handler(void){
   printk(KERN_INFO "modleds: loading\n");
   printk(KERN_INFO "modleds: fgconsole is %x\n", fg_console);
   return vc_cons[fg_console].d->port.tty->driver;
}


/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}


long ledctl(unsigned int leds){

    if(leds > 0x7){
        return -EINVAL;
    }
    else{
		unsigned char bit0 = (leds & 0x1);
		unsigned char bit1 = (leds >> 1) & 0x1;
		unsigned char bit2 = (leds >> 2) & 0x1;

		unsigned int mask = (bit1 << 2) | (bit2 << 1) | bit0;

        return set_leds(kbd_driver, mask);
    }
}

static ssize_t myproc_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	unsigned int n;
	char kbuf[MAX_K];

	int available_space = MAX_K-1;

	if ((*off) > 0) /* The application can write in this entry just once !! */
    	return 0;
  
	if (len > available_space) {
		printk(KERN_INFO "modlist: not enough space!!\n");
		return -ENOSPC;
	}

	/* Transfer data from user to kernel space */
	if (copy_from_user(kbuf, buf, len))  
		return -EFAULT;

	if(sscanf(kbuf, "%x", &n) == 1){
		ledctl(n);
	}else{
		return -EINVAL;
	}

	*off+=len;  
	
	return len;
}

struct proc_ops pops = {
    .proc_write = myproc_write, //write()
};

static int __init modleds_init(void)
{	
//Crear Entrada en /proc
	proc_entry = proc_create( "modlist", 0666, NULL, &pops);
    if (proc_entry == NULL) {
      printk(KERN_INFO "Modlist: Can't create /proc entry\n");
	  return -ENOMEM;
    } else {
      printk(KERN_INFO "Modlist: Module loaded\n");
    }
   kbd_driver= get_kbd_driver_handler();
//    ledctl(ALL_LEDS_ON);

   return 0;
}

static void __exit modleds_exit(void){
	remove_proc_entry("modlist", NULL);
    set_leds(kbd_driver, ALL_LEDS_OFF);
}

module_init(modleds_init);
module_exit(modleds_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modleds");
