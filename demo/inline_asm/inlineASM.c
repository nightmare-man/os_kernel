//使用gcc -Og -m32 inlineASM.c -o test.out 编译 可以直接linux下运行
char *str="hello,world\n";
int count=0;
void main(){
    asm("pusha;\
        movl $4,%eax;\
        movl $1,%ebx;\
        movl str,%ecx;\
        movl $12,%edx;\
        int $0x80;\
        movl %eax,count;\
        popa");
}
//AT&T汇编中 将数字默认认定位内存地址 只有加$才认为是立即数
//因此 对于str标号，访问标号对应的内存地址所在的数据，也就是
//在AT&T汇编中 标号更加贴近于变量的用法，是访问变量的内容而不
//地址，而在INTEL汇编中，则是地址