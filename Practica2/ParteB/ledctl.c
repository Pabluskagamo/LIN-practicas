#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/module.h> 
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>


struct tty_driver* kbd_driver=NULL;

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


SYSCALL_DEFINE1(ledctl,unsigned int, leds){

    kbd_driver=get_kbd_driver_handler();
    
    return ledctl(leds);
}   


