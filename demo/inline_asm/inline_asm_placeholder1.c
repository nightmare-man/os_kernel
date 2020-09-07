#include <stdio.h>
int main(){
    int in_a=18,in_b=3,out=0;
    asm("divb %[divisor];movb %%al,%[result]":[result]"=m"(out):"a"(in_a),[divisor]"m"(in_b));
    //用符号占位符 作用和数字占位符类似
    printf("result is %d\n",out);
    return 0;
}