//被编译为 kernel.bin gcc main.c -c -m32 -o main.o ;
//ld main.o print.o -melf_i386 -Ttext 0xc0001500 -e main -o kernel.bin
//kernel.bin 即内核用dd命令写入硬盘第9扇区
#include "../lib/kernel/print.h"
int main(){
    int i=0;
    while(1){
        sleep(40);
        put_char('0'+i);
        i++;
        if(i==10){
            i=0;
        }
    }
    return 0;
}
