#include "../lib/user/syscall.h"
#include "../lib/kernel/stdint.h"
#include "../thread/thread.h"
#include "../lib/kernel/print.h"
#include "../device/console.h"
#include "../lib/string.h"
#define syscall_nr 32
#define _syscall0(NUMBER) ({int retval;asm volatile("int $0x80":"=a"(retval):"a"(NUMBER):"memory");retval;})


#define _syscall3(NUMBER,ARG1,ARG2,ARG3) ({int retval;asm volatile("int $0x80":"=a"(retval):"a"(NUMBER),"b"(ARG1),"c"(ARG2),"d"(ARG3):"memory");retval;})
//实际上 只用_syscall3即可 其余0-2均可被_syscall3代替 原因是在
//int 0x80后调用的handler中，总是将edx（参数三） ecx（参数二） ebx（参数一）入栈
//因此不论syscall_table中的函数用不用 都一样，因此 一个_syscall3就够了
typedef void* syscall;
syscall syscall_table[syscall_nr];
uint32_t sys_getpid(void){
	return running_thread()->pid;
}
uint32_t sys_write(char*str){
	console_put_str(str);
	return strlen(str);
}
void syscall_init(void){
	put_str("syscall_init start\n");
	syscall_table[SYS_GETPID]=(void*)sys_getpid;//往syscall_table里写入sys_getpid的地址
	syscall_table[SYS_WRITE]=(void*)sys_write;
	put_str("syscall_init done\n");
}
uint32_t getpid(void){
	return _syscall0(SYS_GETPID);
}
uint32_t write(char*str){
	return _syscall3(SYS_WRITE,str,0,0);
}
