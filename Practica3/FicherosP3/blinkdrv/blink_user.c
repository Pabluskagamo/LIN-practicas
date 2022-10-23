#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

unsigned int colors[]={0x9400D3, 0x4B0082, 0x0000FF, 0x00FF00,0xFFFF00,0xFF7F00, 0xFF0000, 0x00FF00};

int main (int argc, char* argv[]){

    int fd = 0;

    fd = open("/dev/usb/blinkstick0", O_RDWR);

    if(fd == -1){
        perror("ERROR");
        return -1;
    }

    int color_cnt = 0;
    while(1){
        char *buff=(char*)malloc(100*sizeof(char));
        int count = sprintf(buff, "0:%x,1:%x,2:%x,3:%x,4:%x,5:%x,6:%x,7:%x", colors[0],colors[1],colors[2],colors[3],colors[4],colors[5],colors[6],colors[7]);

        printf("%s \n", buff);

        write(fd, buff, strlen(buff));

        int aux = colors[0];
        for(int i = 0; i<7; i++){
            colors[i] = colors[i+1];
        }
        colors[7] = aux;

        usleep(80*1000);
    }

    return 0;
}