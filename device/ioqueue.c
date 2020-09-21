#include "./ioqueue.h"
#include "../lib/kernel/stdint.h"
#include "../thread/sync.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/interrupt.h"
#include "../thread/thread.h"
//以下函数对ioqueue初始化
void ioqueue_init(struct ioqueue* ioq){
	lock_init(&ioq->lock);
	ioq->consumer=ioq->producer=NULL;
	ioq->head=ioq->tail=0;
}
//以下函数返回pos的下一个位置
static int32_t next_pos(int32_t pos){
	return (pos+1)%buffsize;
}

bool ioq_full(struct ioqueue* ioq){
	ASSERT(intr_get_status()==INTR_OFF);
	return (next_pos(ioq->head)==ioq->tail);
}
bool ioq_empty(struct ioqueue* ioq){
	ASSERT(intr_get_status()==INTR_OFF);
	return (ioq->head==ioq->tail);
}
//二级指针  waiter保存的是task_struct地址的地址
//以下函数 让线程消费者或者生成者等待
//指针的好处是 作为参数可以修改指针指向的那个东西
//现在用的是指针的指针 那我们想修改的当然是 一级指针的值
static void ioq_wait(struct task_struct** waiter){
	ASSERT(*waiter==NULL&&waiter!=NULL);
	*waiter=running_thread();
	thread_block(TASK_BLOCKED);
}
//以下函数 唤醒等待线程
static void wakeup(struct task_struct**waiter){
	ASSERT(*waiter!=NULL);
	thread_unblock(*waiter);
	*waiter=NULL;
}
char ioq_getchar(struct ioqueue*ioq){
	ASSERT(intr_get_status()==INTR_OFF);//很奇怪的写法 为什么还要用关中断而不是再用一把锁？
	
	//这里的锁 只锁了ioq->consumer 对buf的原子操作 则是关中断实现。。
	while(ioq_empty(ioq)){
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->consumer);//修改ioq的consumer指针值
		lock_release(&ioq->lock);
	}
	char byte=ioq->buf[ioq->tail];
	ioq->tail=next_pos(ioq->tail);
	if(ioq->producer!=NULL){
		wakeup(&ioq->producer);
	}
	return byte;
}
void ioq_putchar(struct ioqueue *ioq,char byte){
	ASSERT(intr_get_status()==INTR_OFF);
	while(ioq_full(ioq)){
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->producer);
		lock_release(&ioq->lock);
	}
	ioq->buf[ioq->head]=byte;
	ioq->head=next_pos(ioq->head);
	if(ioq->consumer!=NULL){
		wakeup(&ioq->consumer);
	}
}
