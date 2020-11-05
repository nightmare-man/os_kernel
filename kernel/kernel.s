;;;此模块用汇编语言定义了0-32号中断的处理程序
;;;并在.data节生成了一个名为intr_entry_table全局地址数组
;;;对应处理程序的入口地址
;;; nasm kernel.s -f elf32 -o kernel.o

[bits 32]
%define ERROR_CODE nop
;%define 用于定义单行宏 如果该异常会压入 error_code 那么我们就什么都不做   
%define ZERO push 0
;如果该异常不压入 error_code 那么我们就压入一个0 因为我们的中断处理程序是相同的，
;所以假设都压入中断码 在iret之前 要跳过中断码，所以为了统一，没有中断码我们就自己
;压入一个
extern put_str;声明外部标号（函数）
extern idt_table;使用idt_table数组 里面有真正handler的地址


section .data
intr_str db "interrupt occur!",0x0a,0
global intr_entry_table
intr_entry_table:


;;;;以下是处理程序宏 多行宏 依次是: %macro 宏名称 宏参数个数
%macro VECTOR 2
section .text
intr_%1_entry:  ;使用宏参数产生不同的处理程序标号
%2;由传入参数决定是不是要手动平衡error_code 栈


push ds
push es
push fs
push gs

pushad

mov al,0x20; 0010 0000  通过第3 4位均为0表示发送的是OCW2（operation command word2） sl为0 eoi为1
;表示发送结束当前正在处理的中断的信号
out 0xa0,al;发送给从片
out 0x20,al;发送给主片

push %1;传入中断号 不管handler用不用
call [idt_table+ %1 *4];间接跳转到handler入口地址

jmp intr_exit

section .data
dd intr_%1_entry ;将处理程序有效地址写到.data节

%endmacro
section .text
global intr_exit
intr_exit:
add esp,4;跳过传入的参数 中断号
popad
pop gs
pop fs
pop es
pop ds
add esp,4 ;跳过error code
iret

;;;;利用宏产生处理程序
VECTOR 0x00,ZERO   ;使用宏 参数之间用,隔开 使用ZEOR还是ERROR_CODE取决于该cpu处理该中断号时
;是否压入错误码，压入则用ERROR_CODE 否则使用ZERO 手动补上，具体可查表

;处理中断只做两件事：1 分配中断号    2编写每个中断号的处理程序，并配置idt表
;前0-19中断号都已经被cpu分配了 20-31也保留了，我们要给这些已经分配出去的中断号写对应的处理程序
;另外，我们要配置pic（可编程中断控制器）8259a 来配置rtc中断，所以把32号中断码分配了，所以这里也
;要为32号中断码配置处理程序
;综上 我们一共需要配置33个中断处理程序，0-32中断号的，查表得 只有8 10 11 12 13 14 17这几个会由
;cpu压入error_code，其余全用ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0A,ERROR_CODE
VECTOR 0x0B,ERROR_CODE
VECTOR 0x0C,ERROR_CODE
VECTOR 0x0D,ERROR_CODE
VECTOR 0x0E,ERROR_CODE
VECTOR 0x0F,ZERO
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO
VECTOR 0x18,ZERO
VECTOR 0x19,ZERO
VECTOR 0x1A,ZERO
VECTOR 0x1B,ZERO
VECTOR 0x1C,ZERO
VECTOR 0x1D,ZERO
VECTOR 0x1E,ZERO
VECTOR 0x1F,ZERO
VECTOR 0x20,ZERO;//时钟中断 主片IR0
VECTOR 0x21,ZERO;//键盘8042中断 主片IR1 0X21



;以下为syscall_handler 即0x80中断处理程序及数据结构
[bits 32]
extern syscall_table;调用c语言里定义的全局表
section .text
global syscall_handler;给interrupt.c里init idt_desc_init用
syscall_handler:
push 0;（不需要errorcode 强行push0 保持栈格式一致）
push ds
push es
push fs
push gs
pushad
push 0x80;(不需要压入中断号，强行保持一致)
push edx;参数三
push ecx;参数二
push ebx;参数一 实际上这三个已经在pushad里入栈了 再入栈一次是为了按照cdecl 约定给函数传参数

call [syscall_table+eax*4];调用syscall——table里的函数
add esp,12;跳过传入的参数

mov [esp+8*4],eax;将syscall_table返回值eax 写入到栈里eax的地方（eax在pushad时入栈了 intr_exit会将eaxpop为esp+8*4位置的值）
jmp intr_exit
section .data
dd syscall_handler