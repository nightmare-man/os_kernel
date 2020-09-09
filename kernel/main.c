//被编译为 kernel.bin gcc main.c -c -m32 -o main.o ;
//ld main.o print.o -melf_i386 -Ttext 0xc0001500 -e main -o kernel.bin
//kernel.bin 即内核用dd命令写入硬盘第9扇区
#include "../lib/kernel/print.h"
#include "../lib/kernel/interrupt.h"
extern char trans_table;//在print.s中 trans_table是一个全局的 .data段的的标号，
//标号实际会被汇编器翻译成一个地址，而地址里的数据 也就是我们在标号后面跟上的db 后的
//数据。因此 我们可知 c语言里的全局变量 就是 会汇编里的 全局标号， 全局变量和全局标号
//都是对某个内存地址的别名，里面的数据都可以通过访问这个地址对应的空间得到
//所以 trans_table对应的变量类型是char而不是char* 我们这里说的类型是这个别名对应的数据
//的类型，而不是说别名本身是什么
int main(){
    
    idt_init();
    while(1);
    return 0;
}
