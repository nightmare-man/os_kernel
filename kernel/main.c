//被编译为 kernel.bin gcc main.c -c -m32 -o main.o ;
//sld main.o -melf_i386 -Ttext 0xc0001500 -e main -o kernel.bin
//kernel.bin 即内核用dd命令写入硬盘第9扇区
int main(){
    while(1);
    return 0;
}
