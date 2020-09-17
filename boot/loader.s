%include "include/boot.inc"
LOADER_STACK_TOP equ LOADER_BASE_ADDR
SECTION loaders vstart=LOADER_BASE_ADDR
jmp near loader_start

;---GDT
GDT_BASE dd 0x00000000;0号描述符
         dd 0x00000000;
CODE_DESC dd 0x0000FFFF;段描述符低32位
        dd DESC_CODE_HIGH4 ;段描述符高32位 基地址0 段界限 0xfffff 4k粒度 因此范围0-4GB
DATA_STACK_DESC dd 0x0000FFFF;
        dd  DESC_DATA_HIGH4;同样是0-4GB 向上扩展（段界限作为上界检查）的数据段 但是没关系啦，向下扩展的栈段也可以使用
VIDEO_DESC dd 0x80000007; 基地址0xb8000 低32位的高16位是基地址的低16位 为 0x8000   范围 是0xb8000-0xbffff ,因此段界限是0x8000
;使用4K粒度是 段界限填0x07即可 （0x07+1）*0x1000-1
        dd DESC_VIDEO_HIGH4
GDT_SIZE equ $-GDT_BASE
GDT_LIMIT equ  GDT_SIZE-1

times 120 dd 0;预留60个段描述符


SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

ards_buf times 240 db 0;缓冲区 保存返回结构
ards_nr dw 0;统计返回多少个结构

loader_msg db "2 loader in real.",0

loader_start:

;----------------------------------------loader 开始工作 实模式下
mov ax,0
mov es,ax

xor ebx,ebx;清0
mov edx,0x534d4150 ;签名
mov di,ards_buf;es:di 为返回结构的缓冲地址
.e820_loop:
mov eax,0xe820
mov ecx,20;返回的结构大小
int 0x15
jc .check_end;如果cf为1 则读取失败
add di,20;更新缓冲地址
inc word [ards_nr];统计结构个数
cmp ebx,0
jne .e820_loop;cf为0 ebx非0 说明还有结构没读 如果 cd为0 ebx也为0 那么说明读完了

;这个结构返回的是物理内存的布局 每个区域的基地址+长度，所以最大容量 是 地址的最大值 即 基地址+长度的最大值 以字节为单位，因此要用32为寄存器保存
;遍历所有结构 找到最大的；  （32位平台下只考虑 结构的0字节偏移（低32位基地址）和8字节偏移（长度的低32位））
xor ebx,ebx;用ebx保存最大值
mov di,ards_buf;es di 是结构数组的起始地址
mov cx,[es:ards_nr];循环次数
.max_loop:
xor eax,eax
mov eax,[es:di];
add eax,[es:di+8];
cmp eax,ebx
ja .above
add di,20
loop .max_loop
jmp .check_end
.above:
mov ebx,eax;更新最大值
add di,20
loop .max_loop


.check_end:

mov ax,0
mov es,ax
mov ds,ax
mov ss,ax
mov sp,LOADER_BASE_ADDR

mov [MEM_BYTES_TOTAL_ADDR],ebx;写入内存大小

mov bp,loader_msg;es:bp 待显示字符串
mov cx,17
mov ax,0x1301;设置功能号 及显示方式
mov bx,0x001f;设置颜色
mov dx,0x1800;设置行列
int 0x10;

;----------------------------------------进入保护模式
;打开A20
in al,0x92
or al,0000_0010b
out 0x92,al

;---加载gdt
lgdt [gdt_ptr]

;---打开cr0 pe位
mov eax,cr0
or eax,0x01
mov cr0,eax


;----------------------------------------刷新流水线里的指令 刷新各个段寄存器及缓存部分
jmp dword SELECTOR_CODE:p_mode_start


[bits 32]
p_mode_start:
mov ax,SELECTOR_DATA
mov ss,ax
mov ds,ax
mov es,ax
mov fs,ax
mov ax,SELECTOR_VIDEO
mov gs,ax;gs指向video段




;----------------------------------------读入 kernel.bin
mov eax,KERNEL_START_SECTOR
mov esi,KERNEL_BIN_BASE_ADDR
mov ecx,200
call rd_disk_32

call setup_page
sgdt [gdt_ptr];重新写回gdtr的值
mov ebx,[gdt_ptr+2];获取gdt基地址
or dword [ebx+0x18+4],0xc0000000;将视频段描述符中的基地址+0xc0000000 实际上，只需要将高32位中的最高8位中的最高2位置1
;另外两个段描述符不用改，因为它们是0-4GB不是针对物理地址建立的，而视频段则是针对物理地址0x000b8000建立的

or dword [gdt_ptr+2],0xc0000000;修改gdt的基地址
or esp,0xc0000000;同样的，要修改esp ，因为开启分页机制后，全部会使用虚拟地址

mov eax,PAGE_DIR_TABLE_POS
mov cr3,eax;cr3中高20位是物理地址 低12位是属性 但是实际上 属性全0，因此直接使用eax

mov eax,cr0
or eax,0x80000000
mov cr0,eax;开启分页机制

lgdt [gdt_ptr];开启分页机制后，重新加载gdt


call kernel_init
;mov esp 0x9f000这是main_thread tcb的最高地址 表明了 在执行main线程
mov esp,0x9f000
jmp KERNEL_ENTRY_POINT



;----------------------------------------以下是调用例程
;----------------------------------------
;----------------------------------------加载内核段

kernel_init:
xor eax,eax
xor ebx,ebx;ebx用来记载程序头表的偏移地址 program headers table 【elf文件偏移0x20 四字节】
xor ecx,ecx; cx用来记载头表的数量          【elf文件偏移0x38 两字节】 
xor edx,edx;dx记载e_phentsize,即头表的大小 【elf文件偏移0x36 两字节】

mov dx,[KERNEL_BIN_VIRTUAL_ADDR + 0x2a]
mov ebx,[KERNEL_BIN_VIRTUAL_ADDR +0x1c]
add ebx,KERNEL_BIN_VIRTUAL_ADDR
mov cx,[KERNEL_BIN_VIRTUAL_ADDR+0x2c]

.each_segment:
;检测segment的type是不是load，且必须加载的目标地址要是0xc0000000以上的（因为其他虚拟地址暂时未分配！）
cmp dword [ebx+0],PT_LOAD
jne .ptnull
test dword [ebx+8],0x80000000
jz .ptnull
test dword [ebx+8],0x40000000
jz .ptnull

push dword [ebx+20];压入segment大小
mov eax,[ebx+4];压入segment在文件里的起始偏移地址
add eax,KERNEL_BIN_VIRTUAL_ADDR;拿到segment在缓冲区的虚拟地址
push eax;压入要复制的起始地址
push dword [ebx+8];压入segment想要的的虚拟地址
call mem_cpy
add esp,12;跳过三个参数
.ptnull:
add ebx,edx
loop .each_segment
ret
;----------------------------------------进行内存复制 （dst，src，size）es edi ds esi ecx，通过push传入 stdcall
mem_cpy:
cld;df标志位置0 正向复制
push ebp
mov ebp,esp

push ecx                                                 
mov edi,[ebp+8];   [参数三][参数二][参数一][ret addr][ebp][ecx]
                                                   ; 个 ebp指向[ebp] ，+8指向 参数一（stdcall，右边参数先push 在栈底）
mov esi,[ebp+12]
mov ecx,[ebp+16]
rep movsb;rep movsb 串复制 每次复制1byte 从 ds si ->es edi 由于是平坦模型 ds es都一样段起始地址都是0x00000000
pop ecx
pop ebp
ret
;----------------------------------------创建页目录及页表

setup_page:
mov ecx,4096
mov esi,0
.clear_page_dir:
mov byte [PAGE_DIR_TABLE_POS+esi],0
inc esi
loop .clear_page_dir

.create_pde:
mov eax,PAGE_DIR_TABLE_POS
add eax,0x1000 ;第一个页表
mov ebx,eax

or eax,PG_US_U|PG_RW_W|PG_P
mov [PAGE_DIR_TABLE_POS+0x00],eax ;给第一个PDE写入对应页表地址及属性
mov [PAGE_DIR_TABLE_POS+0xc00],eax;将这个页表也挂到内核地址空间的第一个PDE （内核是3GB以上） 1100 0000 00_00 0000 0000 0000 0000 0000
;内核所在的虚拟地址空间第一个pde对应的索引是 0x300,因此第一个pde的偏移是0xc00
sub eax,0x1000
mov [PAGE_DIR_TABLE_POS+4092],eax ;eax是PDT的物理地址 将PDT的最后一个PDE（1023*4）指向自己，方便以后用虚拟地址访问PDT

;以下对第一个PDE对应的页表，建立页表项
mov ecx,256 ;只建立前256个，因为我们只用到了1MB地址空间，只分配256个
mov esi,0;分配的物理页的初始地址0x00
mov edx,PG_US_U|PG_RW_W|PG_P
.create_pte:
mov [ebx+esi*4],edx
add edx,0x1000;分配的物理页地址,每次增加4096
inc esi
loop .create_pte

;创建内核其他的 所有的 PDE 为什么现在就分配，就创建？
;因为内核是共享的，高于3GB以上的线性地址空间和物理地址的映射
;在所有进程里都要相同，因此 如果不一开始就分配好，以后分配
;就需要修改所有进程的PDE

;虽然分配了页表，但是每个页表都是空的
mov eax,PAGE_DIR_TABLE_POS
add eax,0x2000
or eax,PG_US_U|PG_RW_W|PG_P
mov ebx,PAGE_DIR_TABLE_POS
mov ecx,254;一共有256个 但是最高的PDE用来指向自己 第一个内核PDE已经创建了
mov esi,769
.create_kernel_pde:
mov [ebx+esi*4],eax
inc esi
add eax,0x1000
loop .create_kernel_pde
ret

rd_disk_32:; 保护模式下的32位读磁盘 eax LBA扇区号 ds:esi 读入内存的地址 ecx 读入的扇区数
    push ecx
    push ds
    push esi
    push edi
    mov ebp,eax;//使用32位寄存器
    mov edi,ecx;//对eax cx备份

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

    mov eax,edi
    mov edx,256
    mul edx
    mov ecx,eax;//16位*16位 结果 dx：ax ax是低16位 保存着需要读取的次数
    ;一个扇区512字节 需要读 256次（1次16位）
    mov dx,0x1f0
.go_on_read:
    in ax,dx;这里是x一次读16位，16位的端口而不是8位
    mov [ds:esi],ax
    add esi,2
    loop .go_on_read
    pop edi
    pop esi
    pop ds
    pop ecx
    ret
