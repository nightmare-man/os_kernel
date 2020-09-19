#ifndef _HTREAD_SYNC_H
#define _THREAD_SYNC_H
#include "../lib/kernel/list.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/stdint.h"
struct semaphore{
	uint8_t value;
	struct list waiters;
};
struct lock{
	struct semaphore semaphore;
	struct task_struct* owner;
	uint32_t holder_repeat_nr;//用于记录owner重复申请lock的次数
};

#endif