;使用 nasm -f elf32 编译成.o文件等待与另一个链接
section .data  ;手动实现.data和.text节
str: db "asm_print says hello world",0x0a,0; 0x0a 是换行符 0是结束符
str_len equ $-str

section .text 
extern c_print;使用extern关键字引用外部符号
global _start;global生成外部符号
_start:
    push str
    call c_print
    add esp,4;c编译器 是默认调用者释放传入参数
    mov eax,1
    int 0x80;结束
global asm_print
asm_print:
    push ebp
    mov ebp,esp
    mov eax,4
    mov ebx,1
    mov ecx,[ebp+8];[第二个参数][第一个参数][返回地址][ebp]；
    mov edx,[ebp+12]
    int 0x80
    pop ebp
    ret