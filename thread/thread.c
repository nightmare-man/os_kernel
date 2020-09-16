#include "./thread.h"
#include "../lib/kernel/stdint.h"
#include "../lib/string.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/memory.h"



static void kernel_thread(thread_func func,void*arg){
	func(arg);
}
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

void init_thread(struct task_struct* pthread,char*name,int prio){
	memset(pthread,0,sizeof(struct task_struct));
	strcpy(pthread->name,name);
	pthread->status=TASK_RUNNING;
	pthread->priority=prio;
	pthread->self_kstack=(uint32_t*)((uint32_t)pthread+PG_SIZE);//self_kstack 是线程自己使用的栈的栈顶，初始状态应该是PCB（process 
	//control block）的最高处,所以是分配的地址（最低处）+页大小
	pthread->stack_magic=0x19980114;//定义的栈边界检测 魔数
}

struct task_struct* thread_start(char*name,int prio,thread_func func,void*func_arg){
	struct task_struct* thread=get_kernel_page(1);//分配空间
	init_thread(thread,name,prio);
	thread_create(thread,func,func_arg);
	
	asm volatile("movl %0,%%esp;pop %%ebp;pop %%ebx;pop %%edi;pop %%esi;ret"::"g"(thread->self_kstack):"memory");
	//根据前面的thread_create,thread->self_kstack已经指向了thread_stack的最低地址，因此pop 的四个寄存器刚好对应
	//结构里保存的四个寄存器，然后ret（注意不是retf 不用是段间），那么会将thread_stack->ip作为地址返回，而thread_create
	//中已经将其设置为kernel_thread的入口地址了，所以自然是ret到了该函数，同时巧妙的是！！！我们来到该函数时 栈顶是调用kernel
	//_thread的返回地址 栈顶+4 是参数一 对应thread_stack结构里的 func +8是参数二 对应结构里的arg，虽然我们不是通过call调用的
	//但是我们在thread_create里通过对thread_stack的复制构建了这一情况，使得我们ret到kernel_thread时 传入了正确的参数 ，能够
	//正确的执行函数 函数回去调用我们传入的函数及参数！
	return thread;
}