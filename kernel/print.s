%include "../boot/include/boot.inc"


;.text节包含两个函数 符号是global 可以供调用 是静态链接 
;编译命令 nasm print.s -f elf32 -o print.o
;再和调用者链接再一起
;可以引用 os_kernel/lib/kernel/print.h 包含函数声明和数据定义
[bits 32]
section .text
global sleep
sleep:
push eax
push ebx
mov eax,[esp+12];访问传入的参数


xor ebx,ebx

.sleep_loop:
sub bx,1
sbb ax,0
cmp bx,0
jne .sleep_loop
cmp ax,0
jne .sleep_loop
pop ebx
pop eax
ret

global put_char
put_char:
pushad
mov ax,SELECTOR_VIDEO
mov gs,ax;设置gs 

mov dx,0x03d4;controller register address register  ;用于写入要访问的寄存器序号
mov al,0x0e;获取光标位置的高8位
out dx,al
mov dx,0x03d5;数据端口
in al,dx
mov ah,al;放在ah

mov dx,0x03d4
mov al,0x0f;获取光标位置低8位
out dx,al
mov dx,0x03d5
in al,dx;此时ax即光标位置 80*25中的索引位置

xor ebx,ebx
mov bx,ax
mov cl,[esp+36];[char][1][2]..[8][ret addr]所以加36
cmp cl,0x0d;判断是不是回车
je .is_carriage_return
cmp cl,0x0a;判断是不是换行
je .is_line_feed
cmp cl,0x08
je .is_backspace
jmp .put_other
;;;;;

.is_backspace:
dec bx;回光标
shl bx,1;找到对应的显存偏移
mov byte [gs:bx],0x20;清除
inc bx
mov byte [gs:bx],0x07;显色属性恢复默认
shr bx,1;光标位置
jmp .set_cursor

;;;;

.put_other:
shl bx,1
mov [gs:bx],cl
inc bx
mov byte [gs:bx],0x07
shr bx,1
inc bx
cmp bx,2000
jl .set_cursor;小于2000设置光标，反之滚屏
jmp .roll_screen
;;;;;
.is_carriage_return:
mov si,80
xor dx,dx
mov ax,bx
div si
mul si
mov bx,ax
jmp .set_cursor
;;;;;
.is_line_feed:
add bx,80
cmp bx,2000
jnb .roll_screen
;;;;;;
.roll_screen:;从第二开始 往前一行复制 一直到最后一行
mov esi,0xb8000
add esi,160;从第二行开始复制
mov edi,0xb8000
add edi,0;往第一行复制
cld
mov ecx,1920
rep movsw;1920个字节
mov ecx,80
mov esi,0xb8000
add esi,3840;最后一行开头
.cls:;清除最后一行
mov byte [esi],0x20
inc esi
mov byte [esi],0x07
inc esi
loop .cls
sub bx,80
jmp .set_cursor
;;;;;

.set_cursor:;将光标设置位bx
mov dx,0x03d4
mov al,0x0e
out dx,al

mov dx,0x03d5
mov al,bh;先设置高8位
out dx,al

mov dx,0x03d4
mov al,0x0f
out dx,al

mov dx,0x03d5
mov al,bl;再设置低8位
out dx,al
jmp .put_char_done

.put_char_done:
popad
ret
