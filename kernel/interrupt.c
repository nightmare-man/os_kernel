/*
    此模块定义了idt，并往里面写入了0-32的表项
    提供了idt_init方法 安装中断描述符，加载ldt，初始化8259a（暂未实现）
    编译 gcc -c -m32 interrupt.c -o interrupt.o
*/

#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/io.h"
#include "../thread/thread.h"
#include "../lib/kernel/timer.h"
#include "../lib/kernel/debug.h"
#include "../lib/user/tss.h"
//#include "global.h"
#define IDT_DESC_CNT 0x81 //目前支持的中断数 0-0x80 0x80为syscall 中断
//2020-9-20 加入idt中键盘中断
#define EFLAGS_IF 0x00000200 //IF位为1
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl;popl %0":"=g"(EFLAG_VAR))
//g 表示分配任意寄存器 = 表示可读


#define PIC_M_CTRL 0x20 //8259a主片 控制 端口
#define PIC_M_DATA 0x21 //数据端口
#define PIC_S_CTRL 0xa0 //从片 控制端口
#define PIC_S_DATA 0xa1 //从片数据端口
extern struct tss tss;



//以下是中断门描述符格式 共8字节
struct gate_desc{
    uint16_t func_offset_low_word;//中断处理程序偏移地址低16位
    uint16_t selector;//中断处理程序代码段选择子
    uint8_t dcount;//此8位未使用
    uint8_t attribute;//属性8位
    uint16_t func_offset_high_word;//中断处理程序偏移地址高16位
};
static struct gate_desc idt[IDT_DESC_CNT];//static的本质是在符号表（symbol tabl）里为local，链接时不能被其他模块引用


char* intr_name[IDT_DESC_CNT];//对每个中断做一个说明
intr_handler idt_table[IDT_DESC_CNT];//保存每个handler的入口地址

extern intr_handler intr_entry_table[IDT_DESC_CNT];//0-32中断向量（中断入口地址，这些入口最终还是调用
//上面的handler）



//------------------------------------------以下是handler
//开始写handler
static void general_intr_handler(uint8_t vec_nr){
    if(vec_nr==0x27||vec_nr==0x2f){
        return ;//IRQ7和IRQ15 主片和从片最后一个引脚会产生伪中断，不用处理
    }

	set_cursor(0);//光标置左上角
	//清屏前4行
	int cursor_pos=0;
	while(cursor_pos<320){
		put_char(' ');
		cursor_pos++;
	}
	set_cursor(0);
	put_str("!!!     exception message begin     !!!\n");
	set_cursor(88);
	put_int((uint32_t)vec_nr);
	put_str(intr_name[vec_nr]);
	if(vec_nr==14){
		int page_fault_vaddr=0;
		asm volatile("movl %%cr2,%0":"=r"(page_fault_vaddr));//cr2中报错了 引起缺页的访问地址
		put_str("\npage fault addr is:");
		put_int((uint32_t)page_fault_vaddr);

	}
	put_str("\n!!!     exception message end     !!!\n");
	while(1);//有中断直接死循环了 ；由于进入中断处理器自动将if置0 因此可屏蔽中断关了 出不去了
}



//------------------------------以下是时钟中断设置

void register_handler(uint8_t vector,intr_handler function){
	idt_table[vector]=function;
	//因为idt表里的处理程序都是调用idt_table里的handler 所以想改直接
	//改idt_table
}



//-------------------------------以下是一般中断设置




static void exception_init(void){
    int i;
    for(i=0;i<IDT_DESC_CNT;i++){
        idt_table[i]=general_intr_handler;
        intr_name[i]="unknow";//先统一为unknow 再逐个修改
    }
    intr_name[0]="#DE divide error";
    intr_name[1]="#DB debug exception";
    intr_name[2]="NMI Interrupt";
    intr_name[3]="#BP breakpoint exception";
    intr_name[4]="#OF overflow exception";
    intr_name[5]="#BR bound range exceeded exceprion";
    intr_name[6]="#UD invalid opcode exception";
    intr_name[7]="#NM device not availiable exception";
    intr_name[8]="#DF double fault exception";
    intr_name[9]="#Coprocessor segment overrun";
    intr_name[10]="#TS Invalid tss exception";
    intr_name[11]="#NP segment not present";
    intr_name[12]="#SS stack fault exception";
    intr_name[13]="#GP general protection exception";
    intr_name[14]="#PF page-fault exception";
    //第15号中断被保留  不用说明
    intr_name[16]="#MF fpu error";
    intr_name[17]="#AC alignment check exception";
    intr_name[18]="#MC machine-check exception";
    intr_name[19]="#XF simd error";
}







static void make_idt_desc(struct gate_desc* p_gdesc,uint8_t attr,intr_handler function){	
    p_gdesc->func_offset_low_word=((uint32_t)function) & 0x0000ffff;
    p_gdesc->selector=SELECTOR_K_CODE;
    p_gdesc->dcount=0;
    p_gdesc->attribute=attr;
    p_gdesc->func_offset_high_word=(((uint32_t)function)&0xffff0000)>>16;
}
static void idt_desc_init(void){
    int i;
	int lastindex=(IDT_DESC_CNT-1);//0X80 SYS_CALL
    for(i=0;i<0x22;i++){
        make_idt_desc(&idt[i],IDT_DESC_ATTR_DPL0,intr_entry_table[i]);
    }
	put_int((uint32_t)intr_entry_table[0x22]);
	put_char('\n');
	make_idt_desc(&idt[lastindex],IDT_DESC_ATTR_DPL3,intr_entry_table[0x22]);
	//硬件中断不检查特权级，因此idt_desc的dpl怎么设置都行
	//但是软中断（int调用的） 要检查dpl 因此设置为3 
	//但是目标代码段仍然是内核代码段 特权级为0，中断和调用门均可从特权级3转移到0 （依从代码可执行 但是特权级不变）
	//而唯一降低权限的方法是ret返回（为什么不允许主动call 降权，因为主动call降权，就意味着ret可以升权，那么任意特权级3的代码都可以
	//构造栈来主动ret升权了）
    put_str("idt_desc_init done!\n");
}
static void pic_init(void){

    //初始化主片
    outb(PIC_M_CTRL,0x11);//ICW1 边沿触发 级联  使用icw4
    outb(PIC_M_DATA,0x20);//ICW2 PIC控制的中断号的起始地址 0x20 (32起 0-19cpu用了 20-31保留了 
    //，而且起始地址 只能设置高5位 必须为8的倍数)
    outb(PIC_M_DATA,0x04);//ICW3 主片IR2接从片 0000 0100
    outb(PIC_M_DATA,0x01);//不自动结束中断 8086模式

    //初始化从片
    outb(PIC_S_CTRL,0x11);
    outb(PIC_S_DATA,0x28);//从片的中断号 延续主片
    outb(PIC_S_DATA,0x02);//告诉从片字节被链接到了主片的IR2上
    outb(PIC_S_DATA,0x01);

    //主片 1111 1000 0xf8（打开ir0 时钟 ir1 键盘 ir2 从片） 从片 1011 1111 0xbf(打开ir14 ata0 ata0_master和ata0_slave都在这个中断)
    outb(PIC_M_DATA,0xf8);//OCW1 完成ICW初始化后 对0x21/0xa1 的第一个写入即是OCW1 用于屏蔽IR 
    outb(PIC_S_DATA,0Xbf);//从片全关
    put_str("pic_init done!\n");
}
void idt_init(){
    put_str("idt_init start\n");
    idt_desc_init();//初始化中断表 这里初始化只将入口地址指向kernel.s里的intr_entry_table数组里的入口地址
    //那些入口地址对应的处理例程会跳转到 idt_table数组里地址对应的handler
    exception_init();//初始化idt_table 数组

    pic_init();//初始化8259a
    uint64_t idt_operand = (sizeof(idt)-1)|(  ((uint64_t)((uint32_t)idt) )<<16  );//sizeof(idt)-1即是idt界限
	//idt是描述符表
    asm volatile("lidt %0"::"m"(idt_operand));
    put_str("idt_init done!\n");
}

enum intr_status intr_get_status(void){
	uint32_t eflags=0;
	GET_EFLAGS(eflags);
	return (eflags & EFLAGS_IF)?INTR_ON:INTR_OFF;
}
enum intr_status intr_enable(void){
	enum intr_status old_status;
	if(intr_get_status()==INTR_ON){
		old_status=INTR_ON;
		return old_status;
	}else{
		old_status=INTR_OFF;
		asm volatile("sti;");//打开中断
		return old_status;
	}
}
enum intr_status intr_disable(void){
	enum intr_status old_status;
	if(intr_get_status()==INTR_ON){
		old_status=INTR_ON;
		asm volatile("cli;");//关闭中断
		return old_status;
	}else{
		old_status=INTR_OFF;
		return old_status;
	}
}
//设置新状态 返回老状态
enum intr_status intr_set_status(enum intr_status status){
	return status&INTR_ON?intr_enable():intr_disable();
}