#include "./console.h"
#include "../lib/kernel/print.h"
#include "../thread/sync.h"
#include "../thread/thread.h"
#include "../lib/kernel/stdint.h"
static struct lock console_lock;
void console_init(){
	put_str("\n console_init start\n");
	lock_init(&console_lock);
	put_str("console_init done\n");
}
void console_accquire(){
	lock_acquire(&console_lock);
}
void console_release(){
	lock_release(&console_lock);
}
void console_put_str(char* str){
	console_accquire();
	put_str(str);
	console_release();
}
void console_put_int(uint32_t n){
	console_accquire();
	put_int(n);
	console_release();
}
void console_put_char(char n){
	console_accquire();
	put_char(n);
	console_release();
}
