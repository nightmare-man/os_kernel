#include "../lib/kernel/stdint.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/io.h"
#include "./keyboard.h"
static void keyboard_handler(void){
	put_char('k');
	inb(KBY_BUF_PORT);//读缓冲区 不然8042不会继续接收键盘按键 也不会有新中断
	return;
}
void keyboard_init(void){
	put_str("keyboard_init start");
	register_handler(0x21,keyboard_handler);
	put_str("keyboard_init done\n");
}
