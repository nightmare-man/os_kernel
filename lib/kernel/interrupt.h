#ifndef _INTERRUPT_H
#define _INTERRUPT_H
#include "./stdint.h"

typedef void* intr_handler;
void idt_init();
enum intr_status{
	INTR_OFF,
	INTR_ON
};//两种状态 如果强制类型转换成int 0，1
enum intr_status intr_get_status(void);
enum intr_status intr_enable(void);
enum intr_status intr_disable(void);
enum intr_status intr_set_status(enum intr_status status);
void timer_init();
void register_handler(uint8_t vector,intr_handler function);//注册中断函数
#endif
