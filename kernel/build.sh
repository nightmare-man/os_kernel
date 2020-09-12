gcc -c -m32 -fno-stack-protector main.c -o ../build/main.o
gcc -c -m32 -fno-stack-protector init.c -o ../build/init.o
gcc -c -m32 -fno-stack-protector interrupt.c -o ../build/interrupt.o
gcc -c -m32 -fno-stack-protector ../device/timer.c -o ../build/timer.o
gcc -c -m32 -fno-stack-protector debug.c -o ../build/debug.o
nasm -f elf32 kernel.s -o ../build/kernel.o
nasm -f elf32 print.s -o ../build/print.o
ld ../build/main.o ../build/init.o ../build/debug.o ../build/interrupt.o ../build/kernel.o ../build/print.o ../build/timer.o -melf_i386 -Ttext 0xc0001500 -e main -o ../build/kernel.bin
#ld -Ttext 0xc0001500 是指定链接后的代码段的起始位置是0xc0001500 而-e main 则是指定elf的entry为main符号对应的地址

dd if=../build/kernel.bin of=../../HD60M.img bs=512 count=200 seek=9 conv=notrunc
cd ~
bochs -f bochs.disk
