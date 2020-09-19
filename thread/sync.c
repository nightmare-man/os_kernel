#include "./sync.h"
#include "../lib/kernel/interrupt.h"
#include "./thread.h"
void sema_init(struct semaphore*psema,uint8_t value){
	psema->value=value;
	list_init(&psema->waiters);
}
void lock_init(struct lock*plock){
	plock->owner=NULL;
	plock->holder_repeat_nr=0;
	sema_init(&plock->semaphore);
}
void sema_down(struct semaphore*psema){
	//信号量本身也是一种公共资源 访问必须要原子操作
	enum intr_status old_status=intr_disable();
	while(psema->value==0){//为什么用while而不是if？ 因为 可能第一判断不行  然后阻塞 回来后还应该判断 所以用while判断
		ASSERT(!elem_find(&psema->waiters,running_thread()->general_tag));//当前进程不应该在等待队列
		//（等待队列里状态都是block等）
		list_append(&psema->waiters,running_thread()->general_tag);//psema->value=0无法拿到锁 直接阻塞 加入等待队列
		thread_block(TASK_BLOCKED);
	}
	psema->value--;
	ASSERT(psema->value==0);//拿到锁 互斥锁 二元信号
	intr_set_status(old_status);
}
void seme_up(struct semaphore*psema){
	enum intr_status old_status=intr_disable();
	ASSERT(psema->value==0);//二元信号量增加必定是从0增加
	if(!list_empty(&psema->waiters)){
		struct task_struct*thread_blocked =elem2entry(struct task_struct,general_tag,list_pop(&psema->waiters));
		//取出 阻塞线程最前一个tcb 并unblock
		thread_unblock(thread_blocked);
	}
	psema->value++;
	ASSERT(psema->value==1);
	intr_set_status(old_status);
}