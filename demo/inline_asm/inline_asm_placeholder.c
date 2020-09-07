#include <stdio.h>
int main(){
    int in_a=0x12345678,in_b=0;
    asm("movw %1,%0":"=m"(in_b):"a"(in_a));//AT&T汇编 源操作数在左，目的操作数在右
    printf("word in_b is 0x%x\n",in_b);
    in_b=0;

    asm("movb %1,%0":"=m"(in_b):"a"(in_a));//%1 与 %0都是占位符 代表 output/input 部分的实际载体（变量对应的寄存器或者内存空间）
    printf("low byte in_b is 0x%x\n",in_b);
    in_b=0;

    asm("movb %h1,%0":"=m"(in_b):"a"(in_a));//默认会根据指令中指定的数据大小来裁剪数据，但是8位的数据有 低8位和高8位可用，所以要指定h高8位
    //同样的，还有h b w k 一共四种操作码，用来指定使用寄存器时，用哪一部分 h是 低16位的高8位 b是低8位 w是低16位  k是32位全部
    printf("high byte in_b is 0x%x\n",in_b);
    return 0;
}