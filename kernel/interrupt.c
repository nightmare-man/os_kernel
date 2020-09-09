/*
    此模块定义了idt，并往里面写入了0-32的表项
    提供了idt_init方法 安装中断描述符，加载ldt，初始化8259a（暂未实现）
    编译 gcc -c -m32 interrupt.c -o interrupt.o
*/

#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/global.h"
#include "../lib/kernel/print.h"
//#include "global.h"
#define IDT_DESC_CNT 0x21 //目前支持的中断数

//以下是中断门描述符格式 共8字节
struct gate_desc{
    uint16_t func_offset_low_word;//中断处理程序偏移地址低16位
    uint16_t selector;//中断处理程序代码段选择子
    uint8_t dcount;//此8位未使用
    uint8_t attribute;//属性8位
    uint16_t func_offset_high_word;//中断处理程序偏移地址高16位
};
static struct gate_desc idt[IDT_DESC_CNT];//static的本质是在生成的符号时为gobal，链接时不能被其他模块引用
extern intr_handler intr_entry_table[IDT_DESC_CNT];//0-32号中断处理向量数组

static void make_idt_desc(struct gate_desc* p_gdesc,uint8_t attr,intr_handler function){
    p_gdesc->func_offset_low_word=((uint32_t)function) & 0x0000ffff;
    p_gdesc->selector=SELECTOR_K_CODE;
    p_gdesc->dcount=0;
    p_gdesc->attribute=attr;
    p_gdesc->func_offset_high_word=(((uint32_t)function)&0xffff0000)>>16;
}
static void idt_desc_init(void){
    int i;
    for(i=0;i<IDT_DESC_CNT;i++){
        make_idt_desc(&idt[i],IDT_DESC_ATTR_DPL0,intr_entry_table[i]);
    }
    put_str("    idt_desc_init done!\n");
}

void idt_init(){
    put_str("idt_init start\n");
    idt_desc_init();//初始化中断表
  //  pic_init();//初始化8259a
    uint64_t idt_operand = (sizeof(idt)-1)|(  ((uint64_t)((uint32_t)idt) )<<16  );//sizeof(idt)-1即是idt界限
    asm volatile("lidt %0"::"m"(idt_operand));
    put_str("idt_init done!\n");
}