#include <stdio.h>
int fun_real(void){
	printf("this is real func\n");
	return 0;
}
int (*func1)(void);//定义一个 返回值为int 无参数的 函数变量 func1
typedef int (*func_type)(void);//定义一个 返回值为int 无参数 的函数类型 func type
func_type func2;//根据函数类型 定义一个变量

typedef int func_type1(void);
func_type1* func3;//这和上面的是一样的 只不过写法不同
int main(){
	func1=fun_real;
	func2=fun_real;
	func3=fun_real;
	func1();
	func2();
	func3();
	//均可执行 效果一样
	return 0;
}