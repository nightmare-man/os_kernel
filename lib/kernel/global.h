#ifndef _GLOBAL_H
#define _GLOBAL_H
#include "./stdint.h"
#define MEM_BYTES_TOTAL_ADDR 0x1f0
#define GDT_LINE_ADDR 0xc0000903 //这个地址是之前loader.s里我们把描述符都随便保存到了一个位置 那个位置的标号地址是0x903
//以下是选择子
#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

#define TI_GDT 0
#define TI_LDT 1

#define SELECTOR_K_CODE ((1<<3)+(TI_GDT<<2)+RPL0)
#define SELECTOR_K_DATA ((2<<3)+(TI_GDT<<2)+RPL0)
#define SELECTOR_K_STACK SELECTOR_K_DATA
#define SELECTOR_K_GS ((3<<3)+(TI_GDT<<2)+RPL0)
//第四个GDT描述符留给tss
#define SELECTOR_U_CODE ((5<<3)+(TI_GDT<<2)+RPL3)
#define SELECTOR_U_DATA ((6<<3)+(TI_GDT<<2)+RPL3)
#define SELECTOR_U_STACK SELECTOR_U_DATA




//以下为GDT描述符属性
#define DESC_G_4K 1
#define DESC_D_32 1
#define DESC_L 0//64位平台使用 在此置0
#define DESC_AVL 0//闲置位
#define DESC_P 1
#define DESC_DPL_0 0
#define DESC_DPL_1 1
#define DESC_DPL_2 2
#define DESC_DPL_3 3

//系统段 ldt tss以及各种门描述符 s为0 其余数据代码段 s为1
#define DESC_S_CODE 1
#define DESC_S_DATA DESC_S_CODE
#define DESC_S_SYS 0
#define DESC_TYPE_CODE 8
// x=1 c=0 r=0 a=0 可执行 非依从 不可读 访问位被清0
#define DESC_TYPE_DATA 2
//x=0 e=0 w=1 a=0 不可执行 向上扩展 可写 访问位被清0
#define DESC_TYPE_TSS 9// 	Busy 位为0 任务不忙



//以下为中断门描述符属性：
#define IDT_DESC_P 1
#define IDT_DESC_DPL0 0
#define IDT_DESC_DPL3 3//就 0 3两种常用
#define IDT_DESC_32_TYPE 0XE//32位中断门的type字段


#define IDT_DESC_ATTR_DPL0 ((IDT_DESC_P<<7)+(IDT_DESC_DPL0<<5)+IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3 ((IDT_DESC_P<<7)+(IDT_DESC_DPL3<<5)+IDT_DESC_32_TYPE)

//以下是gdt中的描述符

//GDT descriptor的高32位的第16-23位
#define GDT_ATTR_HIGH ((DESC_G_4K<<7)+(DESC_D_32<<6)+(DESC_L<<5)+(DESC_AVL<<4))
// 8-15位
#define GDT_CODE_ATTR_LOW_DPL3 \
	((DESC_P<<7)+(DESC_DPL_3<<5)+(DESC_S_CODE<<4)+(DESC_TYPE_CODE))
#define GDT_DATA_ATTR_LOW_DPL3 \
	((DESC_P<<7)+(DESC_DPL_3<<5)+(DESC_S_DATA<<4)+(DESC_TYPE_DATA))

//TSS描述符
#define TSS_DESC_D 0
#define TSS_ATTR_HIGH \
	((DESC_G_4K<<7)+(TSS_DESC_D<<6)+(DESC_L<<5)+(DESC_AVL<<4)+0x00)
//TSS ATTR HIGH 里低四字节是 tss desc limit 的最高4字节 由于tss大小比较小 所以直接写死 0x00
#define TSS_ATTR_LOW \
	((DESC_P<<7)+(DESC_DPL_0<<5)+(DESC_S_SYS<<4)+DESC_TYPE_TSS)
#define SELECTOR_TSS ((4<<3)+(TI_GDT<<2)+RPL0)

struct gdt_desc{
	uint16_t limit_low_word;
	uint16_t base_low_word;
	uint8_t base_mid_byte;
	uint8_t attr_low_byte;
	uint8_t limit_high_attr_high;
	uint8_t base_high_byte;
};

#endif