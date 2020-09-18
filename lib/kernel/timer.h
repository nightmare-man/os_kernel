#ifndef _TIMER_H
#define _TIMER_H
#define IRQ0_FREQUENCY 10000
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE (INPUT_FREQUENCY / IRQ0_FREQUENCY)
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER0_MODE 2
#define READ_WRITE_LATCH 3 
//锁存器 对计数器的读写方式 
#define PIT_CONTROL_PORT 0x43
//控制字寄存器
#include "./stdint.h"
void frequency_set(uint8_t counter_port,uint8_t counter_no,uint8_t rwl,uint8_t counter_mode,uint16_t counter_value);

#endif