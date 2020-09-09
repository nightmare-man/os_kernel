/*
 //使用inline 关键字,写在.h文件里 是为了让函数内联 提高执行速度
//对端口的操作一定要快！！

*/
#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "./stdint.h"


static inline void outb(uint16_t port,uint8_t data){
    asm volatile("outb %b0,%w1"::"a"(data),"Nd"(port));
    //使用inline 关键字,写在.h文件里   让编译器将函数内联，提高执行速度（内核速度要尽可能快）
    // d表示使用寄存器edx dx dl
    //N 表示立即数约束 不使用寄存器和内存 而是直接用立即数替换 ！！暂且不知道什么意思！！
}

static inline void outsw(uint16_t port,void*addr,uint32_t word_cnt){
    asm volatile("cld;rep outsw":"+S"(addr),"+c"(word_cnt):"d"(port));
    
    //outsw 指令 将ds esi处的 cx 个字（两字节）的数据写入端口dx中
    //S（大写）表示esi +让esi 和 cx可读可写 因为在这个过程中 cx不断减小直到0 esi不断增大（因为cld df=0 正向移动）
    //同样 因为esi和cx会改变，所以我们将他们放在output中 
}

static inline uint8_t inb(uint16_t port){
    uint8_t data;
    asm volatile("inb %w1,%b0":"=a"(data):"Nd"(port));
    return data;
}
static inline void insw(uint16_t port,void* addr,uint32_t word_cnt){
    asm volatile("cld;rep insw":"+D"(addr),"+c"(word_cnt):"d"(port):"memory");
    //insw 指令 从端口读cx个字到es edi处 
    //因此 edi改变 cx改变 这两者即要读又要写 因此放在output里 带上+
    //port 放在dx里
    //由于读到内存里 内存改变 modify部分写 "memory"
}

#endif