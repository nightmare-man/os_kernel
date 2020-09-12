#include "../lib/kernel/print.h"
#include "../lib/kernel/interrupt.h"
void panic_spin(char*file,int line,const char*func,const char*condition){
	intr_disable();//首先关闭中断，毕竟ASSERT失败了，没有继续执行的必要
	put_str("\n\n\nerror!\n\n\n");
	put_str("filename:");put_str((char*)file);put_char('\n');
	put_str("line:");put_int(line);put_char('\n');
	put_str("func:");put_str((char*)func);put_char('\n');
	put_str("condition:");put_str((char*)condition);put_char('\n');
	while(1);
}