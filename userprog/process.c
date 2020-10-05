#include "../lib/kernel/stdint.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/global.h"
#include "../thread/thread.h"
#include "../lib/user/process.h"

extern void intr_exit(void);//定义在kernel.s里中断处理程序的返回部分 用于跳转到新进程
//线程模型：
//timer中断处理程序 call schedule函数 schedule 调用 switch.s 里的switch 然后switch利用将栈顶移到thread_stack最低端pop拿到上下文 ret到kernel_thread函数
//kernel_thread函数调用thread_start里传入的函数（这个调用是thread_stack栈里的上下文准备的）

//而进程模型则是在schedule时检查时新执行流是进程还是线程。如果是进程 就准备进程的页表，并更新esp0（切换进程内核态的上下文）
//然后还是上面那个流程 只不过kerneal_thread 传入的再是start_process 和 filename_ 作为start_process的参数
//这个start_process会设置好intr_stack 也就是中断返回时的栈的信息，然后将esp更新到intr_stack的最底端pop拿到上下文
//通过中断的iret返回 实现特权级的切换  （能过切换特权级的就只有 中断返回和任务返回，ret不行）,到用户代码段执行（特权级3）

//对于线程的切换 认为ret到kernel_thread时就到了新线程了
//对于用户进程同样是，到了kernel_thread就到了该进程的内核态
//执行完start_process后就到了进程的用户态
void start_process(void* filename_){
	void*function=filename_;
	struct task_struct*cur=running_thread();
	cur->self_kstack+=sizeof(struct thread_stack);//在创建线程时（进程模型也使用）的create_thread 将self_kstack设置为tcb最顶端
	//减去intr_stack thread_stack大小，让其刚好指向thread_stack最低端，以便后面switch拿到这个地址更新esp 切换执行流
	//现在将这个self_kstack+ struct thread_stack 让其指向intr_stack最低端，
	struct intr_stack*proc_stack=(struct intr_stack*)cur->self_kstack;
	proc_stack->edi=proc_stack->esi=proc_stack->ebp=proc_stack->esp_dummy=0;
	proc_stack->ebx=proc_stack->edx=proc_stack->ecx=proc_stack->eax=0;
	proc_stack->gs=0;
	proc_stack->ds=proc_stack->es=proc_stack->fs=SELECTOR_U_DATA;
	proc_stack->eip=function;
	proc_stack->cs=SELECTOR_U_CODE;
	proc_stack->eflags=(EFLAGS_IOPL_0|EFLAGS_MBS|EFLAGS_IF_1);
	proc_stack->esp=(void*)((uint32_t)get_a_page(PF_USER,USER_STACK3_VADDR)+PG_SIZE);
	proc_stack->ss=SELECTOR_U_DATA;
	asm volatile("movl %0,%%esp;jmp intr_exit"::"g"(proc_stack):"memory");//更新esp0 切换到用户进程的内核态的intr_stack
	//从此栈pop拿到上下文 回到新进程的特权级3 代码和特权级3的栈 和数据段
}
