#include <linux/module.h>	/* Requerido por todos los módulos */
#include <linux/kernel.h>	/* Definición de KERN_INFO */
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/list.h>

MODULE_LICENSE("GPL"); 	/*  Licencia del modulo */

#define MAX_K       PAGE_SIZE


//Entradas en /proc
static struct proc_dir_entry *proc_entry;

//Lista
struct list_item{
    int data;
    struct list_head links;
};

struct list_head head;

struct list_head *tail;


static ssize_t myproc_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
  
  char kbuf[MAX_K];
	struct list_item* item = NULL;
	struct list_head* cur_node = NULL;
	
	if ((*off) > 0) /* Tell the application that there is nothing left to read */
		return 0;

	char *wrptr = kbuf;
	int nr_bytes = 0;

	list_for_each(cur_node, &head) {
		item = list_entry(cur_node, struct list_item, links);

		int n_bytes = sprintf(wrptr, "%d\n", item->data);
		wrptr += n_bytes;
		nr_bytes += n_bytes;
	}
		
	if (len < nr_bytes)			
		return -ENOSPC;

	/* Transfer data from the kernel to userspace */  
	if (copy_to_user(buf, kbuf, nr_bytes))
		return -EINVAL;
	

	(*off)+=len;  /* Update the file pointer */

	return nr_bytes; 
}


static ssize_t myproc_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	int n;
	char kbuf[MAX_K];

	int available_space = MAX_K-1;

	struct list_item *it = NULL;
	struct list_head *act_node = NULL;
	struct list_head *aux = NULL;

	printk(KERN_INFO "modlist: %s\n", kbuf);

	if ((*off) > 0) /* The application can write in this entry just once !! */
    	return 0;
  
	if (len > available_space) {
		printk(KERN_INFO "modlist: not enough space!!\n");
		return -ENOSPC;
	}

	/* Transfer data from user to kernel space */
	if (copy_from_user( &kbuf[0], buf, len ))  
		return -EFAULT;


	if(sscanf(kbuf, "add %i", &n) == 1){
		//meter n en la lista
		printk(KERN_INFO "add\n");
		
		it = kmalloc(sizeof(struct list_item), GFP_KERNEL); 
		it->data = n; //meto el numero en it
		
		list_add(&it->links, tail);
		tail = &it->links;

	}
	else if(sscanf(kbuf, "remove %i", &n) == 1){
		//eliminar de la lista
	
		list_for_each_safe(act_node, aux, &head){
			it = list_entry(act_node, struct list_item, links);
			
			if(it->data == n){
				printk(KERN_INFO "Procedo a borrar el %i", n);
				list_del(act_node);
				kfree(it);
			}
			
		}
	}
	else if(strcmp(kbuf, "cleanup\n") == 0){
	 	printk(KERN_INFO "cleanup\n");

		list_for_each_safe(act_node, aux, &head){
			it = list_entry(act_node, struct list_item, links);

			list_del(act_node);					//borrar
			kfree(it); //liberar memoria
		}
	}
	else{
		return -EINVAL;
	}

	*off+=len;  
	
	return len;
}

struct proc_ops pops = {
    .proc_read = myproc_read, //read()
    .proc_write = myproc_write, //write()
};

/* Función que se invoca cuando se carga el módulo en el kernel */
int modulo_lin_init(void)
{
	int ret = 0;
    //Crear Entrada en /proc
	proc_entry = proc_create( "modlist", 0666, NULL, &pops);
    if (proc_entry == NULL) {
      ret = -ENOMEM;
      printk(KERN_INFO "Modlist: Can't create /proc entry\n");
    } else {
      printk(KERN_INFO "Modlist: Module loaded\n");
    }
	
	INIT_LIST_HEAD(&head);
	tail = &head;

	/* Devolver 0 para indicar una carga correcta del módulo */
	return 0;
}

/* Función que se invoca cuando se descarga el módulo del kernel */
void modulo_lin_clean(void)
{
	remove_proc_entry("modlist", NULL);
	printk(KERN_INFO "Modulo LIN descargado. Adios kernel.\n");
}

/* Declaración de funciones init y exit */
module_init(modulo_lin_init);
module_exit(modulo_lin_clean);
