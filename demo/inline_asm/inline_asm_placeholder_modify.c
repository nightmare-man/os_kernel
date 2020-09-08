#include <stdio.h>
int main(){
    int a=6;
    int ret_value=0;
    printf("ret_value is %d\n",ret_value);
    asm("movl %%eax,%0;\
        movl %%eax,%%ebx\
        ":"=m"(ret_value):"a"(a):"bx");//最后写上"bx"是modify部分，表明ebx/bx/bl被修改了  除了寄存器外 还有 "memory"
        //表示内存被修改了，要刷新对内存数据的缓存，当然也可以将内存对应的变量用 volatile 定义 
    printf("ret_value now is %d\n",ret_value);
    return 0;
}