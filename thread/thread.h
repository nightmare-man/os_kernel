#ifndef _THREAD_H
#define _THREAD_H
#include "./stdint.h"
#include "../lib/kernel/list.h"
typedef void(*thread_func)(void*);
//thread_func即是 函数变量类型


//tcb thread control block 分布
// 从低到高 task_struct  thread_stack intr_stack
//线程被创建时 esp初始会指向 tcb最高地址 然后 按照顺序 压入intr相关数据 压入thread 相关数据 
//至此 esp指向task_struct的最高位 一直可以压入数据 直到检测到了stack_magic 就达到了tcb中
//栈的下界 不能继续压入了 不然会破坏tcb中有关线程的信息

//以下定义执行流/线程的几种状态
enum task_status{
	TASK_RUNNING,
	TASK_READY,
	TASK_BLOCKED,
	TASK_WAITING,
	TASK_HANGING,
	TASK_DIED
};


//此栈是中断栈 用于发生中断时保存线程的上下文
struct intr_stack{
	//此结构中的成员偏移位置与中断发生时各数据入栈的顺序相同
	//先入栈的在高地址 详情可见kernel/kernel.s 
	uint32_t vec_no;//压入的中断号
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t esp_dummy;// pushad中会压入esp 但是popad中esp不会被恢复，所以此处仅
	//占位让其保存，没有啥用

	uint32_t ebx;//这些寄存器保存顺序都是约定 属于 abi   二进制数据约定
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;

	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;

	//以下是发生中断时 一些其他可能压入的数据
	uint32_t error_code;
	void (*eip)(void);
	uint32_t cs;
	uint32_t eflags;
	void* esp;
	uint32_t ss;
};

//此栈是线程状态栈 暂时不太清除用途 后面再看看
struct thread_stack{
	//根据栈的用法 总是从高向低扩展 因此 以下结构顺序对应 各数据压入顺序 从下往上
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edi;
	uint32_t esi;
	//根据c语言的abi规范 这四个寄存器由被调用者保存

	void(*eip)(thread_func func,void*func_arg);//
	void(*unsign_retaddr)(void);//
	thread_func function;
	void*func_arg;
//这一个栈不是很理解 先写着 后面再来分析 2020-9-16
};
struct task_struct{
	uint32_t*self_kstack;//用于保存调度中的栈顶，通过这个栈顶的保存恢复来切换栈
	enum task_status status;
	uint8_t priority;
	char name[16];
	uint8_t ticks;//每次被处理器执行的时间 （滴答计数）（倒计时 每次滴答该数-1 到0就被换下,被换上时初值由priority决定）
	uint32_t elapsed_ticks;//该执行流被执行的总时间
	struct list_elem general_tag;//创建一个elem 并根据status放入对应的线程列表（只记录对应状态的线程） 用于被调度
	struct list_elem all_list_tag;//再次创建一个elem，放入总的线程列表（这个列表记录所有线程）
		//为什么需要两个elem 因为要放在两个队列里，而单个elem只有一个前驱一个后继 只能表示在一个列表里的位置
		// 列表里只有elem没有 task_struct 都是通过前面定义的elem2entry反推task_struct的地址的
	
	uint32_t* pgdir;//执行流的页表的虚拟地址 如果为线程 该项为NULL 
	uint32_t stack_magic;
	// 按照栈从上往下 分布的原则 从stack_magic以上到self_kstack之间都是线程使用的栈空间
	//stack_magic检测 在边界防止破坏PCB除栈以外的信息
};

struct task_struct* thread_start(char*name,int prio,thread_func func,void*func_arg);
struct task_struct* running_thread();
void thread_init(void);
void schedule();
void thread_block(enum task_status stat);
void thread_unblock(struct task_struct*tar);
#endif