#ifndef _DEVICE_IOQUEUE_H
#define _DEVICE_IOQUEUE_H
#include "../thread/sync.h"
#include "../lib/kernel/stdint.h"
#include "../thread/thread.h"
#define buffsize 64
struct ioqueue{
	struct lock lock;
	struct task_struct* producer;
	struct task_struct* consumer;
	char buf[buffsize];
	int32_t head;//队首指针
	int32_t tail;//队尾指针
};
void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue*ioq);
void ioq_putchar(struct ioqueue *ioq,char byte);
#endif