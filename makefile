BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500
AS_LIB = -I boot/include/ -I boot/
CC_LIB = -I lib/ -I lib/kernel -I lib/user -I kernel/  #使用-I 指定库文件的好处是 
#如果在.c文件中引用路径为绝对路径的.h文件显然是不显示的
#用相对路径 那么 这个相对是根据gcc命令当前目录的 ，所以如果换地方gcc 编译 就会报错
#最终选用在make里指定gcc时路径 并指定查找.h文件的路径  
AS = nasm
CC = gcc
LD = ld
ASFLAGS = ${AS_LIB} -f elf32
CFLAGS = -m32 ${CC_LIB} -c -fno-builtin -fno-stack-protector 
	#-Wstrict-prototypes \
		-Wmissing-prototypes
#-Wall 显示所有警告 -fno-builtin 关闭内建函数(这样才可以声明标准库里的同名函数 比如exit open等) -fno-stack-protector 关闭栈保护 后面两个w开头的时和函数声明有关
LDFLAGS = -melf_i386 -Ttext ${ENTRY_POINT} -e main #-Map ${BUILD_DIR}/kernel.map
OBJS = ${BUILD_DIR}/main.o ${BUILD_DIR}/init.o ${BUILD_DIR}/interrupt.o ${BUILD_DIR}/debug.o \
	${BUILD_DIR}/print.o ${BUILD_DIR}/kernel.o ${BUILD_DIR}/timer.o ${BUILD_DIR}/string.o ${BUILD_DIR}/bitmap.o ${BUILD_DIR}/memory.o 


#####以下是编译部分
${BUILD_DIR}/main.o:kernel/main.c lib/kernel/print.h lib/kernel/init.h lib/kernel/debug.h lib/string.h
	${CC} ${CFLAGS} $< -o $@
${BUILD_DIR}/init.o:kernel/init.c lib/kernel/print.h lib/kernel/init.h lib/kernel/debug.h lib/kernel/stdint.h\
	lib/kernel/interrupt.h lib/kernel/io.h lib/kernel/timer.h
	${CC} ${CFLAGS} $< -o $@
${BUILD_DIR}/interrupt.o:kernel/interrupt.c lib/kernel/print.h lib/kernel/init.h lib/kernel/debug.h lib/kernel/stdint.h\
	lib/kernel/interrupt.h lib/kernel/io.h lib/kernel/timer.h
	${CC} ${CFLAGS} $< -o $@
${BUILD_DIR}/timer.o:device/timer.c lib/kernel/print.h lib/kernel/init.h lib/kernel/debug.h lib/kernel/stdint.h\
	lib/kernel/interrupt.h lib/kernel/io.h lib/kernel/timer.h
	${CC} ${CFLAGS} $< -o $@
${BUILD_DIR}/debug.o:kernel/debug.c lib/kernel/print.h lib/kernel/init.h lib/kernel/debug.h lib/kernel/stdint.h\
	lib/kernel/interrupt.h lib/kernel/io.h lib/kernel/timer.h
	${CC} ${CFLAGS} $< -o $@
${BUILD_DIR}/string.o:lib/string.c lib/kernel/print.h lib/kernel/init.h lib/kernel/debug.h lib/kernel/stdint.h\
	lib/kernel/interrupt.h lib/kernel/io.h lib/kernel/timer.h lib/string.h
	${CC} ${CFLAGS} $< -o $@
${BUILD_DIR}/bitmap.o:kernel/bitmap.c lib/kernel/print.h lib/kernel/init.h lib/kernel/debug.h lib/kernel/stdint.h\
	lib/kernel/interrupt.h lib/kernel/io.h lib/kernel/timer.h lib/string.h lib/kernel/bitmap.h
	${CC} ${CFLAGS} $< -o $@
${BUILD_DIR}/memory.o:kernel/memory.c lib/kernel/print.h lib/kernel/init.h lib/kernel/debug.h lib/kernel/stdint.h\
	lib/kernel/interrupt.h lib/kernel/io.h lib/kernel/timer.h lib/string.h lib/kernel/bitmap.h lib/kernel/memory.h
	${CC} ${CFLAGS} $< -o $@
#####以下是汇编部分
${BUILD_DIR}/print.o:kernel/print.s
	${AS} ${ASFLAGS} $< -o $@
${BUILD_DIR}/kernel.o:kernel/kernel.s
	${AS} ${ASFLAGS} $< -o $@ 
	#$< 代表第一个依赖文件 $@所有目标文件

####链接
${BUILD_DIR}/kernel.bin:${OBJS}
	${LD} $^ ${LDFLAGS} -o $@
	#$^ 所有依赖文件
.PHONY:mk_dir hd clean all#声明伪目标文件 避免同名(和这几个命令重名)文件
#用来生成build文件夹
hd:
	dd if=${BUILD_DIR}/kernel.bin of=../HD60M.img bs=512 count=200 seek=9 conv=notrunc
clean:
	cd ${BUILD_DIR} && rm -f ./*
# && 用来执行多个命令
build:${BUILD_DIR}/kernel.bin
bochs:
	cd .. && bochs -f bochs.disk
all:build hd bochs
#all 是伪目标文件 依赖其他伪目标文件
	


