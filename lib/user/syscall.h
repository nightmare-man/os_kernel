#ifndef _USER_SYSCALL_H_
#define _USER_SYSCALL_H_
#include "stdint.h"
enum SYSCALL_NR{
	SYS_GETPID
};
uint32_t getpid(void);
void syscall_init(void);

#endif