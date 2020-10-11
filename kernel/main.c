//被编译为 kernel.bin gcc main.c -c -m32 -o main.o ;
//ld main.o print.o -melf_i386 -Ttext 0xc0001500 -e main -o kernel.bin
//kernel.bin 即内核用dd命令写入硬盘第9扇区


//我终于知道怎么样链接 才不会出现多余的segment了，即有gcc -c 生成的.o文件
//和nasm生成的.o文件 链接起来 就不会有多余的segment文件

#include "../device/console.h"
#include "../lib/kernel/init.h"
#include "../lib/kernel/debug.h"
#include "../lib/string.h"
#include "../lib/kernel/bitmap.h"
#include "../lib/kernel/memory.h"
#include "../thread/thread.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/print.h"
#include "../device/ioqueue.h"
#include "../device/keyboard.h"
#include "../lib/user/process.h"
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
void func1(void*str);
void func2();
int main(){
	put_str("\nthis is kernel\n");
	init_all();
	
	thread_start("thread1",31,func1,"this is thread1\n");
	thread_start("thread2",31,func1,"this is thread2\n");
	process_execute(func2,"user_func2");
	intr_enable();
    while(1){
		
	}
    return 0;
}
//！！！本章点名批评作者写的这两个函数！！！
//按照作者自己在书里指导的原话 “原子操作的代码应该越小越好...应该尽可能靠近临界区...
//如果一个线程在开始时就关闭中断，结束前再打开，那么多线程代码会退化成单线程代码”
//对于这些话 我很是认同，然而本章给出的生产者消费者演示代码，也就是下面两个函数
//不正是几乎全程关闭中断吗？听其言观其行，这两个函数不够优雅
//代码不够优雅的主要原因，还是ioq_getchar()这个函数（以及ioq_putchar()）
//他们在内部只使用一把锁，用来锁定缓冲区空/满 时等待的消费者/生成者的归属权
//而对于非空/非满时缓冲区的访问，粗暴的要求这两个函数执行时要关闭中断，以此保证
//缓冲区的原子操作，这就导致无论何处调用 这两个函数 都离不开中断的操作。
//能够做的更好 就是两个函数各再加一把锁，锁定缓冲区 

//同日10.36分更新 是我太年轻了 
//如果对buf加锁，那么会导致死锁 原因在于 进ioq_getchar()函数先要判断buf满/空 如果
//要互斥 那么这之前就要加锁，但是如果加了锁 那么如果buf空 进循环后先拿到consumer的锁
//再阻塞自己 去唤醒生产者，但是。。生产者进ioq_putchar()也要访问buf 也要buf的锁，可是
//buf的锁已经被阻塞的消费者拿走了啊 它也不可能释放（如果说在阻塞之前释放 那岂不是啥都没干就
//释放了 那干嘛还拿锁，）？（但是这一条我没深想 后面再想想？）
//这个时候就死锁了 
void func1(void*str){
	while(1){
		put_str(str);
	}
}
void func2(){
	while(1){
		put_str("this is user_prog\n");
	}
}
