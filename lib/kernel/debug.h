#ifndef _KERNEL_DEBUG_H
#define _KERNEL_DEBUG_H
void panic_spin(char*file,int line,const char*func,const char*condition);
#define PANIC(...) panic_spin(__FILE__,__LINE__,__func__,__VA_ARGS__)
// ...表示可变参数 在宏定义里 用  __VA_ARGS__代替可变参数部分
//在此处 __VA_ARGS__代替的是#condition (可以传入也可以不传入为空)
//#的作用是，将变量以字符串的形式传入 比如#1>2 不是传入整数0 而是字符串 "1>2"
#ifdef NDEBUG
	#define ASSERT(condition) ((void)0)
	//如果定义了NDEBUG 则ASSERT 什么都不做 预编译阶段的判断 而不是运行时的判断
#else
	#define ASSERT(condition) if(condition){}else{PANIC(#condition);}
	//如果没有定义NDEBUG ASSERT 正常工作
#endif


#endif