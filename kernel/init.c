#include "../lib/kernel/init.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/timer.h"
void init_all(){
    put_str("  init  all\n");
    idt_init();
    timer_init();
}