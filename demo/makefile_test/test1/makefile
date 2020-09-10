2.o:2.c
	gcc -c -m32 2.c -o 2.o
1.o:1.c
	gcc -c -m32 1.c -o 1.o
test.bin:1.o 2.o#目标文件:依赖文件 多个依赖文件之间要有空格
	gcc 1.o 2.o -m32 -o test.bin
all:test.bin
	@echo "compile done"
#all相当于伪目标文件  
#完整定义如下
#.PHONY:all
#all:test.bin