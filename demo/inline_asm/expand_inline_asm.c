#include <stdio.h>
int main(){
int in_a=1,in_b=2,out_sum;

asm("addl %%ebx,%%eax":"=a"(out_sum):"a"(in_a),"b"(in_b));
printf("sum is %d\n",out_sum);
return 0;
}
//扩展的内联汇编 寄存器操作多了一个% 表明不是直接使用，而是gcc翻译之后的
//assembly:output:input:modify
//input 部分是将变量传入到对应寄存器 a即使eax/ax/al具体用哪一个取决于
//变量的大小
