#include "./thread.h"
#include "../lib/kernel/stdint.h"
#include "../lib/string.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/list.h"
#include "../lib/kernel/print.h"
struct task_struct*main_thread;//主线程tcb thread control block
struct list thread_ready_list;//就绪队列
struct list thread_all_list;//全部线程队列
static struct list_elem* thread_tag;//零时保存节点

extern void switch_to(struct task_struct*cur,struct task_struct*next);//用汇编写的 switch_to函数 用来切换线程

//下面函数获取当前正在运行的线程，原理是线程运行时esp会在其 task_struct 也就是tcb中 tcb大小为0x1000 对esp取整就是
struct task_struct* running_thread(){
	uint32_t esp;
	asm volatile("movl %%esp,%0":"=g"(esp));
	return (struct task_struct*)(esp&0xfffff000);
}

//以下函数 被调用后会执行线程函数（由switch_to函数调用）
static void kernel_thread(thread_func func,void*arg){
	intr_enable();//执行线程函数前 要开中断 不然无法将其换下
	func(arg);
}

//以下函数对 tcb的 thread_stack进行初始化 thread_stack有线程被执行时的上下文
void thread_create(struct task_struct*pthread,thread_func func,void*arg){
	pthread->self_kstack-=sizeof(struct intr_stack);//给intr_stack 留空间  栈顶往下挪就是留出空间
	pthread->self_kstack-=sizeof(struct thread_stack);//同样 thread_stack

	struct thread_stack* kthread_stack=(struct thread_stack*)pthread->self_kstack;//由于前面往下减了该struct的大小 现在用这个
	//指针进行强制类型转换，那么可以将减小的那片内存区域视为该结构了
	kthread_stack->eip=kernel_thread;
	kthread_stack->function=func;
	kthread_stack->func_arg=arg;
	kthread_stack->ebp=kthread_stack->ebx=kthread_stack->esi=kthread_stack->edi=0;
}

//以下函数对tcb整体进行初始化
void init_thread(struct task_struct* pthread,char*name,int prio){
	memset(pthread,0,sizeof(struct task_struct));
	strcpy(pthread->name,name);
	
	if(pthread==main_thread){//由于main函数实际在被执行 所以初始main函数 其status为running 
	//而其余都初始化成ready，因为其余线程不像main 还没有被执行
		pthread->status=TASK_RUNNING;
	}else{
		pthread->status=TASK_READY;
	}

	pthread->elapsed_ticks=0;//总执行时间初始为0
	pthread->pgdir=NULL;//线程没有页表
	pthread->priority=prio;
	pthread->self_kstack=(uint32_t*)((uint32_t)pthread+PG_SIZE);//self_kstack 是线程自己使用的栈的栈顶，初始状态应该是PCB（process 
	//control block）的最高处,所以是分配的地址（最低处）+页大小
	pthread->stack_magic=0x19980114;//定义的栈边界检测 魔数
}

//以下函数创建一个线程 并将该线程加入到ready链表 all链表 （1 初始tcb 2初始化tcb->thread_task上下文 3加入链表）
struct task_struct* thread_start(char*name,int prio,thread_func func,void*func_arg){
	struct task_struct* thread=get_kernel_page(1);//分配空间
	init_thread(thread,name,prio);
	thread_create(thread,func,func_arg);
	

	//现在start一个thread的方式不是直接让其被执行 而是让其加入ready链表 
	ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));//确保之前不在就绪链表
	list_append(&thread_ready_list,&thread->general_tag);
	ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));//确保之前不在所有节点链表
	list_append(&thread_all_list,&thread->all_list_tag);

	
	
	//以下为历史版本注释：
	//asm volatile("movl %0,%%esp;pop %%ebp;pop %%ebx;pop %%edi;pop %%esi;ret"::"g"(thread->self_kstack):"memory");
	//根据前面的thread_create,thread->self_kstack已经指向了thread_stack的最低地址，因此pop 的四个寄存器刚好对应
	//结构里保存的四个寄存器，然后ret（注意不是retf 不用是段间），那么会将thread_stack->ip作为地址返回，而thread_create
	//中已经将其设置为kernel_thread的入口地址了，所以自然是ret到了该函数，同时巧妙的是！！！我们来到该函数时 栈顶是调用kernel
	//_thread的返回地址 栈顶+4 是参数一 对应thread_stack结构里的 func +8是参数二 对应结构里的arg，虽然我们不是通过call调用的
	//但是我们在thread_create里通过对thread_stack的复制构建了这一情况，使得我们ret到kernel_thread时 传入了正确的参数 ，能够
	//正确的执行函数 函数回去调用我们传入的函数及参数！
	return thread;
}

//以下函数为 为主线程构造tcb并添加到队列
static void make_main_thread(void){
	//我们在loader.s中 进入kernel.bin 执行之前 将mov esp,0x9f000 这就是main_thread task_struct(tcb)的最高地址了
	main_thread=running_thread();
	init_thread(main_thread,"main",31);
	
	//main函数已经被执行了 
	//不需要我们去手动设置它的栈空间来ret到main执行它	
	ASSERT(!elem_find(&thread_all_list,&main_thread->all_list_tag));//不能之前就在所有线程节点链表
	list_append(&thread_all_list,&main_thread->all_list_tag);//加入到所有线程节点链表
	//main线程 处于running状态 不用加入ready链表
}

//以下为调度函数
void schedule(){
	ASSERT(intr_get_status()==INTR_OFF);//调度时必须关中断
	//目前schedule只在时钟中断中被调用 因此不会再被中断 可以保证
	struct task_struct*cur=running_thread();
	if(cur->status==TASK_RUNNING){
		//表示是由于时间片用完了 换下 直接放到ready队列末尾
		ASSERT(!elem_find(&thread_ready_list,&cur->general_tag));//确保之前不在就绪队列
		list_append(&thread_ready_list,&cur->general_tag);
		cur->ticks=cur->priority;//重新填充时间片（下次执行的）
		cur->status=TASK_READY;

	}else{
		//由于blocked等因素被换下 暂时不考虑
	}
	ASSERT(!list_empty(&thread_ready_list));//就绪链表不空 不然没法执行了
	thread_tag=NULL;//临时变量
	thread_tag=list_pop(&thread_ready_list);
	struct task_struct* next=elem2entry(struct task_struct,general_tag,thread_tag);//通过宏（偏移地址计算）得出下一个tcb的位置
	next->status=TASK_RUNNING;
	switch_to(cur,next);//调用切换函数 这个函数用汇编写的

}
void thread_init(void){
	put_str("thread_init start\n");
	list_init(&thread_all_list);//对两个链表进行初始化
	list_init(&thread_ready_list);
	make_main_thread();//给主线程 初始化tcb和加入队列
	put_str("thread_init done\n");
}