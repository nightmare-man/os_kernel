#include "../lib/kernel/init.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/timer.h"
#include "../lib/kernel/memory.h"
#include "../thread/thread.h"
#include "../device/console.h"
#include "../device/keyboard.h"
#include "../lib/user/tss.h"
#include "../lib/user/syscall.h"
void init_all(){
    put_str("  init  all\n");
    idt_init();
	timer_init();
	mem_init();
	thread_init();
	console_init();
	keyboard_init();
	tss_init();
	syscall_init();
}
