%include "boot.inc"
;引入头文件，和c中.h没区别 直接加进来，主要写一些宏
SECTION MBR vstart=0x7c00
    mov ax,0
    mov ds,ax
    mov ss,ax
    mov sp,0x7c00

    call .clear
    mov ax,0
    mov ds,ax
    mov bx,message
    mov dh,1
    mov dl,0
    call .print_string

    mov ax,0
    mov ds,ax
    mov eax,LOADER_START_SECTOR
    mov si,LOADER_BASE_ADDR
    mov cx,4;读4个扇区到内存
    call .rd_disk
    ;以下将print_string的入口地址保存在0x7c00+506,4字节 低2字节段地址 高2字节偏移地址
    mov bx,addr
    mov word [ds:bx],.print_string;不需要再加0x7c00 因为vstart 0x7c00了
    mov word [ds:bx+2],0
    jmp 0:LOADER_BASE_ADDR

.clear:
    push si
    push es
    push cx

    mov si,0xb800
    mov es,si
    mov si,0
    mov cx,2000
.clear_loop:
    mov byte [es:si],0
    add si,2
    loop .clear_loop
    pop cx
    pop es
    pop si
    ret
.print_string:; ds:bx 字符串地址 dh 行 dl 列
    push ax
    push bx
    push cx
    push dx
    push es
    push si

    mov ax,0xb800
    mov es,ax
    mov al,160
    mul dh
    add dl,dl;
    mov dh,0
    add ax,dx
    mov si,ax;si即显存地址

.print_loop:
    mov al,[ds:bx]
    cmp al,0
    je .print_end
    mov [es:si],al
    inc bx
    add si,2
    jmp .print_loop

.print_end:
    pop si
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

.rd_disk:; eax LBA扇区号 ds:si 读入内存的地址 cx 读入的扇区数
    push cx
    push ds
    push si
    push di
    mov ebp,eax;//实模式下还是可以使用32位寄存器
    mov di,cx;//对eax cx备份

    mov dx,0x1f2
    mov al,cl
    out dx,al;写入数据

    mov eax,ebp

    mov dx,0x1f3;从 eax写入0-27位地址到对应端口
    out dx,al

    mov cl,8
    shr eax,cl
    mov dx,0x1f4
    out dx,al

    shr eax,cl
    mov dx,0x1f5
    out dx,al

    shr eax,cl
    and al,0x0f
    or al,0xe0;将4-7位设置成0xe0 使用lba模式
    mov dx,0x1f6
    out dx,al

    mov dx,0x1f7
    mov al,0x20
    out dx,al;读命令

.not_ready:;阻塞式读取
    nop
    in al,dx;0x1f7既用来写命令 又用来查询命令后的状态
    and al,0x88
    cmp al,0x08;如果第3位为1 就准备就绪了 并不需要第7位也为1
    jnz .not_ready;不是0x88就循环等

    mov ax,di
    mov dx,256
    mul dx
    mov cx,ax;//16位*16位 结果 dx：ax ax是低16位 保存着需要读取的次数
    ;一个扇区512字节 需要读 256次（1次16位）
    mov dx,0x1f0
.go_on_read:
    in ax,dx;这里是x一次读16位，16位的端口而不是8位
    mov [ds:si],ax
    add si,2
    loop .go_on_read
    pop di
    pop si
    pop ds
    pop cx
    ret


message db "mbr program is running!",0

times 506-($-$$) db 0
addr  times 4 db 0;用来保存print的地址
db 0x55,0xaa


