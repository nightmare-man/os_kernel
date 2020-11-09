#ifndef _USER_STDIO_H_
#define _USER_STDIO_H_
typedef void* va_list;
#include "../../lib/kernel/stdint.h"
//获取第一个参数的地址（即是v的首地址 va_list是编译器内建类型 实质是 void*）
#define va_start(ap,v) ap=(va_list)&v
//获取下一个参数的值（va_list类型实际是void* 每次指针+4 指向下一个参数，通过传入类型t 强制指针类型转换 再*读出值）
#define va_arg(ap,t) *((t*)(ap+=4))
//将指针置0
#define va_end(ap) ap=NULL
//这些函数都是系统调用 通过中断调用，适合用户进程使用，如果是内核线程，不建议使用，太慢了
void itoa(uint32_t value,char**buf_ptr_addr,uint8_t base);
uint32_t printf(const char*format,...);
uint32_t sprintf(char*buf,const char*format,...);
uint32_t vsprintf(char*str,const char* format,va_list ap);
#endif