;一些宏，预处理指令
;loader的加载地址和磁盘位置
LOADER_BASE_ADDR equ 0x0900
LOADER_START_SECTOR equ 0x02
PAGE_DIR_TABLE_POS equ 0x100000;PDT在1MB内核以外
MEM_BYTES_TOTAL_ADDR equ 0x1f0;没找到合适地方 先把总的物理内存字节数放在这儿
;gdt描述符属性
DESC_G_4K equ 1_000_0000_0_00_0_0000_00000000b
DESC_D_32 equ 100_0000_0_00_0_0000_00000000b
DESC_L equ 0;不设置L位 不是64位系统
DESC_AVL equ 0;不使用AVL位

DESC_LIMIT_CODE2 equ 1111_0_00_0_0000_00000000b;段界限16-19位
DESC_LIMIT_DATA2 equ DESC_LIMIT_CODE2;平坦模型 都是全1，最终合成的段界限0xfffff 粒度4K 即（0xfffff+1）*4096= 4GB 
DESC_LIMIT_VIDEO2 equ 0;显存区域不是平坦模型
DESC_P equ 1_00_0_0000_00000000b;表示该段存在内存里，由cpu负责检查，软件设置
DESC_DPL_0 equ 00_0_0000_00000000b;dpl0
DESC_DPL_1 equ 01_0_0000_00000000b;dpl1
DESC_DPL_2 equ 10_0_0000_00000000b;dpl2
DESC_DPL_3 equ 11_0_0000_00000000b;dpl3
DESC_S_CODE equ 1_0000_00000000b;非系统段 S为1 
DESC_S_DATA equ DESC_S_CODE
DESC_S_SYS equ 0_0000_00000000b;系统段

DESC_TYPE_CODE equ 1000_00000000b;可执行 不可读 非依从 ACCESSED 位由cpu访问后置1
DESC_TYPE_DATA equ 0010_00000000b;不可执行 向上扩展 可写 ACCESSED 0

DESC_CODE_HIGH4 equ (0x00<<24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + DESC_P + DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE + 0X00  ;合成高32位中的中间16位 基地址全0
DESC_DATA_HIGH4 equ (0x00<<24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00   ;平坦模型 4GB 基地址全0
DESC_VIDEO_HIGH4 equ (0x00<<24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + DESC_DPL_3 + DESC_S_DATA +DESC_TYPE_DATA + 0X0b ;

;---选择子 SELECTOR
RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
RPL3 equ 11b
TI_GDT equ 000b
TI_LDT equ 100b

SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL3

;---页表相关
PG_P equ 1b
PG_RW_R equ 00b
PG_RW_W equ 10b
PG_US_S equ 000b
PG_US_U equ 100b

;--加载kernel
KERNEL_START_SECTOR equ 9
KERNEL_BIN_BASE_ADDR equ 0x70000
KERNEL_BIN_VIRTUAL_ADDR equ 0xc0070000
KERNEL_ENTRY_POINT equ 0xc0001500
PT_NULL equ 0
PT_LOAD equ 1