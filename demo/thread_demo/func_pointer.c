#include <stdio.h>
int func1(void){
	printf("this is func1\n");
	return 0;
}
int (*f)(void);
int main(){
	f=func1;
	f();
	printf("%p\n%p\n",f,func1);
	return 0;
}
//我们可以看到f和func1的值都是这个函数的入口地址 所以函数指针实际上就是一个变量 该变量的值是函数的入口地址