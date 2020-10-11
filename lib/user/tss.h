#ifndef _USER_TSS_H
#define _USER_TSS_H
//关于自定义struct 什么时候需要放到.h 文件什么时候需要放到.c文件
//如果该结构会在别的调用.h的.c文件里定义变量 那就在.h文件里 否则还是定义
//.c文件里模块自己用就行了
struct tss{
	uint32_t backlink;//指向调用此任务的任务的tss
	uint32_t*esp0;
	uint32_t ss0;
	uint32_t*esp1;
	uint32_t ss1;
	uint32_t*esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t (*eip)(void);//将eip所在指令定义成一个函数 可以直接执行eip()
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;
	uint32_t cs;
	uint32_t ss;
	uint32_t ds;
	uint32_t fs;
	uint32_t gs;
	uint32_t ldt;
	uint32_t trace;
	uint32_t io_base;
};
void tss_init();
void update_tss_esp(struct task_struct* pthread);
#endif