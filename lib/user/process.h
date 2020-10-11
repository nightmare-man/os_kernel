#ifndef _USER_PROCESS_H
#define _USER_PROCESS_H

#define USER_STACK3_VADDR (0xc0000000-0x1000)
#define USER_VADDR_START (0x8048000)
void process_active(struct task_struct* p_thread);
void process_execute(void* filename,char*name);
#endif