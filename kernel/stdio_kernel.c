#include "../lib/user/stdio.h"
#include "../device/console.h"
void printfk(const char*format,...){
	va_list args;
	va_start(args,format);//根据第一个参数的位置，取得参数列表的位置
	char buf[1024]={0};
	vsprintf(buf,format,args);//这里不能再使用sprintf里 因为本身这个函数是一个不定参数 sprintf也是不定参数的，va_list作为不定参数再
	//传入 会出问题，亲测
	va_end(args);
	console_put_str(buf);
}