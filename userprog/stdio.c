#include "../lib/user/stdio.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/print.h"
//获取第一个参数的地址（即是v的首地址 va_list是编译器内建类型 实质是 void*）
#define va_start(ap,v) ap=(va_list)&v
//获取下一个参数的值（va_list类型实际是void* 每次指针+4 指向下一个参数，通过传入类型t 强制指针类型转换 再*读出值）
#define va_arg(ap,t) *((t*)(ap+=4))
//将指针置0
#define va_end(ap) ap=NULL

//整数转字符串 buf_ptr_addr 是缓冲区的当前位置的指针的指针，用二级指针的原因是，使用了递归，递归过程中要修改递归外面的指针值，因此用
//指针的指针作为参数二 base是进制
//另外插一句，将整数转字符串，我之前一直是while %10 /10这样是倒序输出每一位（低位在前）,因此不得不再用栈push pop调整顺序
//十分不优雅，作者用的递归实现，眼前一亮
void itoa(uint32_t value,char**buf_ptr_addr,uint8_t base){
	uint32_t m=value%base;
	uint32_t i=value/base;

	if(i){
		itoa(i,buf_ptr_addr,base);
	}
	if(m<10){
		*((*buf_ptr_addr)++)=m+'0';
	}else{
		*((*buf_ptr_addr)++)=m-10+'A';
	}
}
uint32_t vsprintf(char*str,const char* format,va_list ap){
	char*buf_ptr=str;
	const char* index_ptr=format;

	//..待续
}