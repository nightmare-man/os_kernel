#include "../lib/user/stdio.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/print.h"
#include "../lib/string.h"
#include "../lib/user/syscall.h"
#include "../fs/file.h"


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
	char index_char=*index_ptr;
	int32_t arg_int;
	char* arg_str;
	while(index_char){
		if(index_char!='%'){
			//普通字符直接复制
			*(buf_ptr++)=index_char;
			index_char=*(++index_ptr);
			continue;
		}
		index_char=*(++index_ptr);
		switch (index_char)
		{
		case 's':
			arg_str=va_arg(ap,char*);
			strcpy(buf_ptr,(const char*)arg_str);
			buf_ptr+=strlen((const char*)arg_str);
			index_char=*(++index_ptr);
			break;
		case 'd':
			arg_int=va_arg(ap,int32_t);
			if(arg_int<0){
				*(buf_ptr++)='-';
				arg_int=0-arg_int;
			}
			itoa(arg_int,&buf_ptr,10);
			index_char=*(++index_ptr);
			break;
		case 'c':
			*(buf_ptr++)=va_arg(ap,char);
			index_char=*(++index_ptr);
			break; 
		case 'x':
			arg_int=va_arg(ap,int32_t);//返回参数列表里下一个int的值
			itoa(arg_int,&buf_ptr,16);
			index_char=*(++index_ptr);
			break;
		
		default:
			break;
		}
	}
	return strlen(str);
}
//利用vsprintf 输出格式化字符串到 终端
uint32_t printf(const char*format,...){
	va_list args;
	va_start(args,format);//根据第一个参数的位置，取得参数列表的位置
	char buf[1024]={0};
	vsprintf(buf,format,args);
	va_end(args);
	return write(stdout_no,buf,strlen(buf));
}

//利用vsprintf 输出格式化字符串到 buf
uint32_t sprintf(char*buf,const char*format,...){
	va_list args;
	va_start(args,format);
	uint32_t retval=vsprintf(buf,format,args);
	va_end(args);
	return retval;
}