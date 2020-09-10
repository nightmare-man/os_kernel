#include "../lib/kernel/timer.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/print.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE (INPUT_FREQUENCY / IRQ0_FREQUENCY)
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER0_MODE 2
#define READ_WRITE_LATCH 3 
//锁存器 对计数器的读写方式 
#define PIT_CONTROL_PORT 0x43
//控制字寄存器


//寄存器初始化
static void frequency_set(uint8_t counter_port,uint8_t counter_no,uint8_t rwl,uint8_t counter_mode,uint16_t counter_value){
    //设置控制字寄存器
    outb(PIT_CONTROL_PORT,(counter_no<<6 | rwl<<4 |counter_mode<<1 | 0));
    //设置计数器初始值 先写低8位 再高8位
    outb(counter_port,(uint8_t)counter_value);
    outb(counter_port,(uint8_t)(counter_value>>8));
}
void timer_init(){
    put_str("timer_init star\n");
    frequency_set(COUNTER0_PORT,COUNTER0_NO,READ_WRITE_LATCH,COUNTER0_MODE,COUNTER0_VALUE);
    put_str("timer_init done\n");
}