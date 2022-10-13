#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#define __NR_ledctl 441


long ledctlinvoke(unsigned int leds){
    return (long) syscall(__NR_ledctl, leds);
}

int main (int argc, char* argv[]){
    unsigned int leds;

    if(argc != 2){
        printf("usage: ./ledctl_invoke <argumento de led_ctl>\n");
        exit(0);
    }

    if(sscanf(argv[1],"%x", &leds) != 1 || leds > 0x7){
        printf("ledctl: Argumento no valido, numero entre 0x0 y 0x7.\n");
        exit(0);
    }
    
    if(ledctlinvoke(leds) == -1){
        perror("ledctl: Error durante la ejecucion de ledctl\n");
        exit(-1);
    }

    return 0;
}