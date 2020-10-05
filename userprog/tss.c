#include "../lib/kernel/stdint.h"
#include "../thread/thread.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/print.h"
#include "../lib/user/tss.h"
#include "../lib/string.h"
#include "../lib/kernel/debug.h"
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
static struct tss tss;

//更新tss中的esp0 为线程的 tcb顶
//更新tss栈顶即是切换进程 因为timer中断会把任何进程/线程带到其内核态（0特权级）使用esp0来切换上下文
void update_tss_esp(struct task_struct* pthread){
	tss.esp0=(uint32_t*)( (uint32_t)pthread+PG_SIZE);
}
//以下函数构造descriptor  值得注意的是 返回值是个结构 对于结构的赋值 和传递 都是内存块复制为基础 并非指针操作
static struct gdt_desc make_gdt_desc(uint32_t*desc_addr,uint32_t limit,uint8_t attr_low,uint8_t attr_high){
	struct gdt_desc ret;
	uint32_t desc_base=(uint32_t)desc_addr;
	ret.base_low_word=(uint16_t)(desc_base&0x0000ffff);
	ret.base_mid_byte=(uint8_t)((desc_base&0x00ff0000)>>16);
	ret.base_high_byte=(uint8_t)((desc_base&0xff000000)>>24);
	ret.limit_low_word=(uint16_t)(limit&0x0000ffff);
	ret.limit_high_attr_high=(uint8_t)((limit&0x000f0000)>>16|(attr_high&0xf0));//有bug这儿
	ret.attr_low_byte=(uint8_t)attr_low;
	return ret;
}
void tss_init(){
	put_str("tss_init start\n");
	uint32_t tss_size=sizeof(tss);
	memset(&tss,0,tss_size);
	tss.ss0=SELECTOR_K_STACK;
	tss.io_base=tss_size;
	//下面这个是直接进行内存复制 把返回的结构体 所占用的内存块 复制到 gdt里给tss desc留的
	//内存块  强制指针类型转换 然后 *操作  gdt线性基地址0xc000 0900 tss是第四个描述符
	// 第0个被占用了 所以是 8*4=0x20 偏移
	*((struct gdt_desc*)(GDT_LINE_ADDR+(SELECTOR_TSS&0xfff8)))=make_gdt_desc( (uint32_t*)&tss,\
	tss_size-1,TSS_ATTR_LOW,TSS_ATTR_HIGH);//这里有一个隐藏问题就是 我们用的TSS_ATTR_HIGH G位是1 也就是4K粒度的
	// 但是实际上我们只需要字节粒度就行了
	
	// //以下创建dpl3 的 代码 数据段 描述符
	*((struct gdt_desc*)(GDT_LINE_ADDR+(SELECTOR_U_CODE&0xfff8)))=make_gdt_desc((uint32_t*)0,0xfffff,(uint8_t)GDT_CODE_ATTR_LOW_DPL3,(uint8_t)GDT_ATTR_HIGH);
	*((struct gdt_desc*)(GDT_LINE_ADDR+(SELECTOR_U_DATA&0xfff8)))=make_gdt_desc((uint32_t*)0,0xfffff,(uint8_t)GDT_DATA_ATTR_LOW_DPL3,(uint8_t)GDT_ATTR_HIGH);

	//由于修改了gdt里的东西 要重新 加载gdt lgdt
	//operand GDT 操作数  用于lgdt
	uint64_t gdt_operand=(((uint64_t)0xc0000903)<<16)+8*7-1;
	ASSERT(gdt_operand==(uint64_t)(0xc00009030037));
	asm volatile("lgdt %0"::"m"(gdt_operand));
	asm volatile("ltr %w0"::"r"(SELECTOR_TSS));
	put_str("tss_init done\n");
}	
