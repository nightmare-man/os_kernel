#include "../lib/kernel/stdint.h"
#include "../thread/thread.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/print.h"
#include "../lib/user/tss.h"
#include "../lib/string.h"
#include "../lib/kernel/debug.h"

struct tss tss;


//在切换到新进程时 什么时候会将esp设置为tss中的esp0？
//我们知道，只有在从低特权级的进程的用户态（特权级3）转移到该进程的内核态（特权级0）才会，才会由cpu自动读tss的esp0
//而我们切换到新进程时，是先切换到该进程的内核态，再由内核态构建上下文返回到其用户态，因此这个过程中处理器不会自动
//给我们将esp指向esp0，因此我们可以在切换到该进程的内核态时，还来得及设置tss的esp0。切换到了新进程的用户态后，再要是
//发生中断，那么处理器就自动设置esp为esp0 

//以下函数设置进程的tss的esp0，而不是立即改变esp，设置了tss的esp0，这样进程在运行时才能正常切换到其内核态，正常响应
//中断
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
