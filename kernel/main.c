//被编译为 kernel.bin gcc main.c -c -m32 -o main.o ;
//ld main.o print.o -melf_i386 -Ttext 0xc0001500 -e main -o kernel.bin
//kernel.bin 即内核用dd命令写入硬盘第9扇区


//我终于知道怎么样链接 才不会出现多余的segment了，即有gcc -c 生成的.o文件
//和nasm生成的.o文件 链接起来 就不会有多余的segment文件

#include "../lib/kernel/print.h"
#include "../lib/kernel/init.h"
#include "../lib/kernel/debug.h"
#include "../lib/string.h"
/*
	在下面的测试中 我将test()函数写在main函数前面，这样的话，test编译后main.o里的位置也在main的前面
	加载到内存空间里也在main函数的前面，所以 通过-Ttext 0xc0001500 链接后的代码段的起始位置是0xc0001500
	而test在代码段最前面 偏移为0，test符号才对应的0xc0001500 而不是main
	至于-e main 只是将elf的entry指向main被分配的线性地址，在此时肯定大于0xc0001500
	所以这个代码有问题 因为我们的loader.s里将kernel.bin的入口地址写死了 0xc0001500 并不是去读elf的entry

	解决方法有2：
			1.改loader.s
			2.不将任何代码至于main函数前面
*/

// void test(){
// 	put_str("this is test\n");
// 	while(1);
// }

//在print.s中 trans_table是一个全局的 .data段的的标号，
//标号实际会被汇编器翻译成一个地址，而地址里的数据 也就是我们在标号后面跟上的db 后的
//数据。因此 我们可知 c语言里的全局变量 就是 会汇编里的 全局标号， 全局变量和全局标号
//都是对某个内存地址的别名，里面的数据都可以通过访问这个地址对应的空间得到
//所以 trans_table对应的变量类型是char而不是char* 我们这里说的类型是这个别名对应的数据
//的类型，而不是说别名本身是什么
int main(){
	char * str="this is a test sentence\n";
	
    put_str("\nI am kernel\n");
    init_all();
   // asm volatile("sti;");
    put_str(str);
	put_str("in above sentence,there are:");
	put_int(strchrs(str,'i'));
	put_str("'i'\n");
    while(1);
    return 0;
}
