#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>

#define BUFSIZE 512

#ifndef SYS_read 
#define SYS_read          0 
#define SYS_open          2 
#define SYS_write         1 
#define SYS_close         3 
#endif


int main(void)
{
    int read_chars=0;
    int fd=0;
    char buf[BUFSIZE+1];
    
    /* Open /proc entry in read-only mode */
    // fd=open("/proc/cpuinfo",O_RDONLY); /asi estaba antes
    fd=syscall(SYS_open,"/proc/cpuinfo",O_RDONLY); //modificacion con syscall
    
    if (fd<0){
        fprintf(stderr,"Can't open the file\n");
        exit(1);
    }
    
    /* Loop that reads data from the file and prints its contents to stdout */
    while((read_chars=syscall(SYS_read,fd,buf,BUFSIZE))>0){
        buf[read_chars]='\0';
        // printf("%s",buf); /asi estaba antes
        syscall(SYS_write, 1, buf, read_chars); //modificacion con syscall            
    }
     
    if (read_chars<0){
        fprintf(stderr,"Error while reading the file\n");
        exit(1);        
    }
    
    	
    /* Close the file and exit */ 
    // close(fd); /asi estaba antes
    syscall(SYS_close, fd); //modificacion con syscall
    return 0;
}
