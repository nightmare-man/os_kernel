#ifndef _USER_SYSCALL_H_
#define _USER_SYSCALL_H_
#include "stdint.h"
enum SYSCALL_NR{
	SYS_GETPID,
	SYS_WRITE,
	SYS_MALLOC,
	SYS_FREE
};
uint32_t getpid(void);
uint32_t write(char*str);
void* malloc(uint32_t size);
void free(void* ptr);
void syscall_init(void);

#endif