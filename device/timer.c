#include "../lib/kernel/timer.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/interrupt.h"
#include "../thread/thread.h"

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
#define mil_secs_per_ticks (1000/IRQ0_FREQUENCY) //每个ticks 的毫秒数

volatile uint32_t ticks;// 系统启动以来的总ticks

static void intr_timer_handler(void){
	struct task_struct* cur_thread=running_thread();
	ASSERT(cur_thread->stack_magic==0x19980114);//对边缘进行检测 如果magic number不对 说明栈溢出修改了

	//ticks相关
	ticks++;
	cur_thread->elapsed_ticks++;
	if(cur_thread->ticks==0){
		//时间片用完 调度新的
		//处理器进入中断 push eflags cs ip 并if tf =0关闭中断了
		//所以能保证调度器是原子操作
		schedule();//schedule 是调度函数
	}else{
		cur_thread->ticks--;
		//不然的话thread ticks减1
	}
}

void timer_init(){
	put_str("timer_init start\n");
	//以下宏都定义在timer.h 可以通过修改IRQ0_FREQUENCY修改频率
	frequency_set(COUNTER0_PORT,COUNTER0_NO,READ_WRITE_LATCH,COUNTER0_MODE,COUNTER0_VALUE);
	register_handler(0x20,intr_timer_handler);
	put_str("timer_init done\n");
}

//寄存器初始化
void frequency_set(uint8_t counter_port,uint8_t counter_no,uint8_t rwl,uint8_t counter_mode,uint16_t counter_value){
    //设置控制字寄存器
    outb(PIT_CONTROL_PORT,(counter_no<<6 | rwl<<4 |counter_mode<<1 | 0));
    //设置计数器初始值 先写低8位 再高8位
    outb(counter_port,(uint8_t)counter_value);
    outb(counter_port,(uint8_t)(counter_value>>8));
}

//以下函数休眠指定个ticks 主动出让cpu
static void ticks_to_sleep(uint32_t sleep_ticks){
	uint32_t start_ticks=ticks;
	while(ticks-start_ticks<sleep_ticks){
		thread_yeild();
	}
}
void milliseconds_sleep(uint32_t mil_secs){
	uint32_t ticks_sleep=DIV_ROUND_UP(mil_secs,mil_secs_per_ticks);
	ASSERT(ticks_sleep>);
	ticks_to_sleep(ticks_sleep);
}
