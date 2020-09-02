//使用gcc -m32 -c 生成.o文件与另一个链接
//最后使用ld -melf_i386 链接两个 生成可执行文件
extern void asm_print(char*,int);
void c_print(char* str){
    int len=0;
    while(str[len++]);
    asm_print(str,len);
}