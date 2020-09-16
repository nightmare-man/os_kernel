#ifndef _THREAD_H
#define _THREAD_H
#include "./stdint.h"
typedef void thread_func(void*);
//thread_func即是 函数变量类型

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
	//根据栈的用法 总是从高向低扩展 因此 以下结构顺序对应 各数据压入顺序 从下往上
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
	uint32_t es;

	//以下是发生中断时 一些其他可能压入的数据
	uint32_t error_code;
	void (*eip)(void);
	uint32_t cs;
	uint32_t eflags;
	void* esp;
	uint32_t ss;
};

//此栈是线程栈 暂时不太清除用途 后面再看看
struct thread_stack{
	//根据栈的用法 总是从高向低扩展 因此 以下结构顺序对应 各数据压入顺序 从下往上
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edi;
	uint32_t esi;
	//根据c语言的abi规范 这四个寄存器由被调用者保存

	void(*eip)(thread_func*func,void*func_arg);//这里定义了一个函数指针 
	//并且这个指针对应的函数 要传入两个参数 第一个又是一个函数指针 第二个是参数
	//分别对应该线程要执行的函数 和传入的参数
	void(*unsign_retaddr)(void);
	thread_func*function;
	void*func_arg;
//这一个栈不是很理解 先写着 后面再来分析 2020-9-16
};
struct task_struct{
	uint32_t*self_lstack;
	enum task_status status;
	uint8_t priority;
	char name[16];
	uint32_t stack_magic;
}

#endif