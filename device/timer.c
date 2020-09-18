#include "../lib/kernel/timer.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/print.h"



//寄存器初始化
void frequency_set(uint8_t counter_port,uint8_t counter_no,uint8_t rwl,uint8_t counter_mode,uint16_t counter_value){
    //设置控制字寄存器
    outb(PIT_CONTROL_PORT,(counter_no<<6 | rwl<<4 |counter_mode<<1 | 0));
    //设置计数器初始值 先写低8位 再高8位
    outb(counter_port,(uint8_t)counter_value);
    outb(counter_port,(uint8_t)(counter_value>>8));
}

//下面这个函数放在interrupt.c中实现 
// void timer_init(){
//     put_str("timer_init star\n");
//     frequency_set(COUNTER0_PORT,COUNTER0_NO,READ_WRITE_LATCH,COUNTER0_MODE,COUNTER0_VALUE);
//     put_str("timer_init done\n");
// }