#include "./string.h"
#include "./kernel/stdint.h"
#include "./kernel/debug.h"

void memset(void*dst_,uint8_t value,uint32_t size){
	ASSERT(dst_!=NULL);
	uint8_t*dst=(uint8_t*)dst_;
	while(size-->0){
		*(dst++)=value;
	}
}
void memcpy(void*dst_,const void* src_,uint32_t size){
	//const void* 表示是一个常量的指针 这个常量的值不能变
	//void* const 则是指针常量 指针不能指向其他
	ASSERT(dst_!=NULL && src_!=NULL);
	uint8_t*dst=(uint8_t*)dst_;
	const uint8_t*src=(const uint8_t*)src_;
	while(size-->0){
		*(dst++)=*(src++);
	}
}
int8_t memcmp(const void*a_,const void * b_,uint32_t size){
	ASSERT(a_!=NULL&&b_!=NULL);
	const uint8_t *a=(const uint8_t*)a_;
	const uint8_t *b=(const uint8_t*)b_;
	while(size-->0){
		if(*a!=*b){
			return ((*a>*b)?1:-1);
		}
		a++;
		b++;
	}
}
char* strcpy(char*dst_,const char*src_){
	ASSERT(dst_!=NULL&&src_!=NULL);
	char*ret=dst_;
	while((*dst_++=*src_++));//既做复制 又用复制的结果来判断是不是结束了
	return ret;
}
uint32_t strlen(const char* str){
	ASSERT(str!=NULL);
	const char*p=str;
	while(*p++);
	return (p-str-1); //因为p是在判断之后+1的 因此会多1
}

int8_t strcmp(const char*a,const char* b){//字典序 a>b ret 1 a=b ret 0 a<b ret -1
	ASSERT(a!=NULL && b!=NULL);
	while(*a!=0 && *a==*b){
		a++;
		b++;
	}
	return *a<*b?-1:*a>*b;//这里的写法 太炫技了 没啥用，反正看编译器优化 老老实实if else不会死的！
	//作者这么写的
}
char* strchr(const char* str,const char ch){
	ASSERT(str!=NULL);
	while(*str!=0){
		if(*str==ch){
			return (char*)str;
		}
		str++;
	}
	return NULL;
}
//找到ch在str中最后一次出现的地址
char* strrchr(const char*str,const char ch){
	ASSERT(str!=NULL);
	const char* last_char=NULL;
	while(*str!=0){
		if(*str==ch){
			last_char=str;
		}
		str++;
	}
	return (char*)last_char;
}
char * strcat(char*dst_,char*src_){
	ASSERT(dst_!=NULL&&src_!=NULL);
	char*dst=dst_;
	while(*dst++);
	dst--;//dst指向原本的\0
	while(*dst++=*src_++);
	return dst_;
}
//找ch在str中出现的次数
uint32_t strchrs(const char*str,const char ch){
	ASSERT(str!=NULL);
	uint32_t ch_cnt=0;
	const char*p=str;
	while(*p!=0){
		if(*p==ch){
			ch_cnt++;
		}
		p++;
	}
	return ch_cnt;
}
