gcc -c -m32 -fno-stack-protector main.c -o ../build/main.o
gcc -c -m32 -fno-stack-protector init.c -o ../build/init.o
gcc -c -m32 -fno-stack-protector interrupt.c -o ../build/interrupt.o
nasm -f elf32 kernel.s -o ../build/kernel.o
nasm -f elf32 print.s -o ../build/print.o
ld ../build/main.o ../build/init.o ../build/interrupt.o ../build/kernel.o ../build/print.o -melf_i386 -Ttext 0xc0001500 -e main -o ../build/kernel.bin
dd if=../build/kernel.bin of=../../HD60M.img bs=512 count=200 seek=9 conv=notrunc
cd ~
bochs -f bochs.disk
