#include <linux/module.h>	/* Requerido por todos los módulos */
#include <linux/kernel.h>	/* Definición de KERN_INFO */
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/rwlock.h>

MODULE_LICENSE("GPL"); 	/*  Licencia del modulo */

#define MAX_K       128

DEFINE_RWLOCK(rwl);

//Entradas en /proc
static struct proc_dir_entry *proc_entry;

//Lista
struct list_item{
    int data;
    struct list_head links;
};

// Lista
struct list_head head;

//Puntero a la cola para facilitar añadir elementos

void addElem(int n){
	struct list_item *it = NULL;

	printk(KERN_INFO "Modlist: Adding %d to list\n", n);
		
	it = kmalloc(sizeof(struct list_item), GFP_KERNEL); 
	it->data = n; //meto el numero en it

	write_lock(&rwl);
	list_add_tail(&it->links, &head);
	write_unlock(&rwl);
}

void removeElem(int n){
	struct list_item *it = NULL;
	struct list_head *act_node = NULL;
	struct list_head *aux = NULL;

	printk(KERN_INFO "Modlist: Removing %d from list\n", n);

	write_lock(&rwl);

	list_for_each_safe(act_node, aux, &head){
		it = list_entry(act_node, struct list_item, links);
			
		if(it->data == n){
			list_del(act_node);
			kfree(it);
			printk(KERN_INFO "Modlist: %d removed from list\n", n);
		}

	}

	write_unlock(&rwl);
}

void cleanupList(void){
	struct list_item *it = NULL;
	struct list_head *act_node = NULL;
	struct list_head *aux = NULL;
	
	write_lock(&rwl);

	list_for_each_safe(act_node, aux, &head){
		it = list_entry(act_node, struct list_item, links);

		list_del(act_node);	//borrar
		kfree(it); //liberar memoria
	}

	write_unlock(&rwl);

	printk(KERN_INFO "Modlist: Cleanup list, List clean.\n");


}


static ssize_t myproc_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
  
  	char kbuf[MAX_K]; 
	struct list_item* item = NULL;
	struct list_head* cur_node = NULL;
	char *wrptr = kbuf;
	char test[10];
	int nr_bytes = 0; //Numero de bytes que ocupa la lista en el buffer
	
	if ((*off) > 0) /* Tell the application that there is nothing left to read */
		return 0;

	read_lock(&rwl);
	list_for_each(cur_node, &head) {
		int n_bytes; //Numero de bytes que ocupa cada elemento de la lista en caracteres

		item = list_entry(cur_node, struct list_item, links);

		n_bytes = sprintf(test, "%d\n", item->data);

		//Comprobacion de que hay espacio en el buffer kbuf.
		if((nr_bytes + n_bytes) >= (MAX_K - 1)){
			printk(KERN_INFO "modlist: No es posible alamacenar mas elementos en el buffer\n");
		}else{
			wrptr += sprintf(wrptr, "%d\n", item->data);
			nr_bytes += n_bytes;
		}
	}
	read_unlock(&rwl);
		
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

	if ((*off) > 0) /* The application can write in this entry just once !! */
    	return 0;
  
	if (len > available_space) {
		printk(KERN_INFO "modlist: not enough space!!\n");
		return -ENOSPC;
	}

	/* Transfer data from user to kernel space */
	if (copy_from_user(kbuf, buf, len))  
		return -EFAULT;

	kbuf[len] = '\0';

	if(sscanf(kbuf, "add %i", &n) == 1){
		addElem(n);
	}
	else if(sscanf(kbuf, "remove %i", &n) == 1){
		removeElem(n);
	}
	else if(strcmp(kbuf, "cleanup\n") == 0){
	 	cleanupList();
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

	/* Devolver 0 para indicar una carga correcta del módulo */
	return ret;
}

/* Función que se invoca cuando se descarga el módulo del kernel */
void modulo_lin_clean(void)
{
	cleanupList(); //Para liberar la memoria de los elementos, si los hay
	remove_proc_entry("modlist", NULL);
	printk(KERN_INFO "Modlist: Module clean\n");
}

/* Declaración de funciones init y exit */
module_init(modulo_lin_init);
module_exit(modulo_lin_clean);
