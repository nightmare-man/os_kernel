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
	sema_init(&plock->semaphore,1);
}
//以下 试图减少二元信号量 P操作
void sema_down(struct semaphore*psema){
	//信号量本身也是一种公共资源 访问必须要原子操作
	enum intr_status old_status=intr_disable();
	// put_str("this thread is:0x");
	// put_int((uint32_t)running_thread());
	// put_char('\n');
	// put_str("this thread status is:0x");
	// put_int((uint32_t)running_thread()->status);
	// put_char('\n');
	while(psema->value==0){//为什么用while而不是if？ 因为 可能第一判断不行  然后阻塞 回来后还应该判断 所以用while判断
		ASSERT(!elem_find(&psema->waiters,&running_thread()->general_tag));//当前进程不应该在等待队列
		//（等待队列里状态都是block等）
		list_append(&psema->waiters,&running_thread()->general_tag);//psema->value=0无法拿到锁 直接阻塞 加入等待队列


		thread_block(TASK_BLOCKED);
	}
	psema->value--;
	ASSERT(psema->value==0);//拿到锁 互斥锁 二元信号
	intr_set_status(old_status);
}
//以下 给二元信号量+1 V操作
void sema_up(struct semaphore*psema){
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

//以下试图获得锁 ，有的持有者 才对该锁对应的公共资源有访问权（试图获得锁而没有持有者都在该锁的阻塞队列）
void lock_acquire(struct lock*plock){

	if(plock->owner!=running_thread()){
		//当前线程不是锁的持有者时
		
	
		sema_down(&plock->semaphore);//P操作 如果锁已经被别人持有了 会阻塞 直到别人释放锁
		plock->owner=running_thread();//执行完P操作 那就表示拿到锁了 
		ASSERT(plock->holder_repeat_nr==0);
		plock->holder_repeat_nr++;
	}else{
		//已经是锁的持有者了 记录申请次数
		plock->holder_repeat_nr++;
	}
}
//以下函数 释放锁 关于 信号量的PV 操作和 owner的设置顺序  P操作时 如果owner设置在前 理论上有信号量没有被减到0 就被V操作的风险
//因为 如果先把锁个一个线程 但是没有P操作 那么如果被中断了 ，内核试图去释放这个锁的时候 就会出问题
//V操作时的顺序恰好相反 先要去除owner
void lock_release(struct lock*plock){



	ASSERT(plock->owner==running_thread());//只能释放自己拥有的锁
	if(plock->holder_repeat_nr>1){
		//如果多次申请了 释放一次是不行的
		plock->holder_repeat_nr--;
		return ;
	}
	ASSERT(plock->holder_repeat_nr==1);
	plock->owner=NULL;//要先置空锁的拥有者
	plock->holder_repeat_nr=0;
	sema_up(&plock->semaphore);//信号量V操作 增大信号 表示资源被释放 
}
