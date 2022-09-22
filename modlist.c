#include <linux/module.h>	/* Requerido por todos los módulos */
#include <linux/kernel.h>	/* Definición de KERN_INFO */
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL"); 	/*  Licencia del modulo */


//Reserva de memoria
//void *kmalloc( size_t size, gfp_t flags);
//void kfree( void *addr );

#define MAX_K       4096
//Entradas en /proc
static struct proc_dir_entry *proc_entry;

static ssize_t myproc_read(struct file *filp, char __user *buf, size_t len, loff_t *off){
	return 0;
}

static ssize_t myproc_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	int n;
	char kbuf[MAX_K];

	int available_space = MAX_K-1;

	if ((*off) > 0) /* The application can write in this entry just once !! */
    	return 0;
  
	if (len > available_space) {
		printk(KERN_INFO "clipboard: not enough space!!\n");
		return -ENOSPC;
	}

	/* Transfer data from user to kernel space */
	if (copy_from_user( &kbuf[0], buf, len ))  
		return -EFAULT;

	if(sscanf(kbuf, "add %i", &n) == 1){
		//meter n en la lista
		printk(KERN_INFO "add\n");
	}
	else if(sscanf(kbuf, "remove %i", &n) == 1){
		//eliminar de la lista
		printk(KERN_INFO "remove\n");
	}
	else if(sscanf(kbuf, "cleanup", &n) == 1){
		printk(KERN_INFO "cleanup\n");
	}
	else{
		return -EINVAL;
	}

	*off+=len;  
	
	return 0;
}

struct proc_ops pops = {
    .proc_read = myproc_read, //read()
    .proc_write = myproc_write, //write()
};


// //Lista
// struct list_item{
//     int data;
//     struct list_head links;
// };

// struct list_head mylist;

// struct list_head {
// 	struct list_head *next, *prev;
// };

// #define LIST_HEAD_INIT(name) { &(name), &(name) }

// #define LIST_HEAD(name) \
// 	struct list_head name = LIST_HEAD_INIT(name)

// #define INIT_LIST_HEAD(ptr) do { \
// 	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
// } while (0)
// /*
//  * These are non-NULL pointers that will result in page faults
//  * under normal circumstances, used to verify that nobody uses
//  * non-initialized list entries.
//  */
// #define LIST_POISON1  ((void *) 0x00100100)
// #define LIST_POISON2  ((void *) 0x00200200)


// /*
//  * Insert a new entry between two known consecutive entries.
//  *
//  * This is only for internal list manipulation where we know
//  * the prev/next entries already!
//  */
// static inline void __list_add(struct list_head *new,
// 			      struct list_head *prev,
// 			      struct list_head *next)
// {
// 	next->prev = new;
// 	new->next = next;
// 	new->prev = prev;
// 	prev->next = new;
// }

// /**
//  * list_add - add a new entry
//  * @new: new entry to be added
//  * @head: list head to add it after
//  *
//  * Insert a new entry after the specified head.
//  * This is good for implementing stacks.
//  */
// static inline void list_add(struct list_head *new, struct list_head *head)
// {
// 	__list_add(new, head, head->next);
// }

// /*
//  * Delete a list entry by making the prev/next entries
//  * point to each other.
//  *
//  * This is only for internal list manipulation where we know
//  * the prev/next entries already!
//  */
// static inline void __list_del(struct list_head * prev, struct list_head * next)
// {
// 	next->prev = prev;
// 	prev->next = next;
// }

// /**
//  * list_del - deletes entry from list.
//  * @entry: the element to delete from the list.
//  * Note: list_empty on entry does not return true after this, the entry is
//  * in an undefined state.
//  */
// static inline void list_del(struct list_head *entry)
// {
// 	__list_del(entry->prev, entry->next);
// 	entry->next = LIST_POISON1;
// 	entry->prev = LIST_POISON2;
// }

// /**
//  * __list_for_each	-	iterate over a list
//  * @pos:	the &struct list_head to use as a loop counter.
//  * @head:	the head for your list.
//  *
//  * This variant differs from list_for_each() in that it's the
//  * simplest possible list iteration code, no prefetching is done.
//  * Use this for code that knows the list to be very short (empty
//  * or 1 entry) most of the time.
//  */
// #define __list_for_each(pos, head) \
// 	for (pos = (head)->next; pos != (head); pos = pos->next)




// void print_list(struct list_head* list){
//     struct list_item* item = NULL;
//     struct list_head* cur_node = NULL;


//     __list_for_each(cur_node, list) {

//         item = list_entry(cur_node, struct list_item, links);
//         //Mostrar elemento en /proc
//     }

// }



/* Función que se invoca cuando se carga el módulo en el kernel */
int modulo_lin_init(void)
{
	int ret = 0;
    //Crear Entrada en /proc
	proc_entry = proc_create( "modlist", 0666, NULL, &pops);
    if (proc_entry == NULL) {
      ret = -ENOMEM;
      printk(KERN_INFO "Clipboard: Can't create /proc entry\n");
    } else {
      printk(KERN_INFO "Clipboard: Module loaded\n");
    }

	/* Devolver 0 para indicar una carga correcta del módulo */
	return 0;
}

/* Función que se invoca cuando se descarga el módulo del kernel */
void modulo_lin_clean(void)
{
	printk(KERN_INFO "Modulo LIN descargado. Adios kernel.\n");
}

/* Declaración de funciones init y exit */
module_init(modulo_lin_init);
module_exit(modulo_lin_clean);