#ifndef _IDE_H_//即integreted driver ecolomics 集成电子驱动
#define _IDE_H_
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/bitmap.h"
#include "../lib/kernel/list.h"
#include "../thread/sync.h"
#include "../lib/kernel/stdio_kernel.h"
#include "../lib/user/stdio.h"

//关于这三个结构的定义顺序，disk中内嵌了partition数组，所以必须先定义partition
//而partition中用了disk*，但是我们要知道，对于指定平台的c程序，所有指针的大小都是一样的（4/8字节），因此可以引用不知道类型的指针
//因此partition要先定义 在定义disk 同理最后定义ide_channel
struct partition{
	uint32_t start_lba;
	uint32_t sec_cnt;//total sector
	struct disk* belong_to;
	struct list_elem part_tag;
	char name[8];//分区名称 
	struct super_block* sb;//本分区的超级块
	struct bitmap block_bitmap;//块位图
	struct bitmap inode_bitmap;//i节点位图
	struct list open_inodes;//本分区打开的i节点队列
};
struct disk{
	char name[8];
	struct ide_channel*my_channel;
	uint8_t dev_no;
	struct partition prim_parts[4];
	struct partition logic_parts[8];//支持最多8个逻辑分区
};

struct ide_channel{
	char name[8];
	uint16_t port_base;
	uint8_t irq_no;
	struct lock lock;
	bool expecting_intr;//
	struct semaphore disk_done;
	struct disk devices[2];//一个通道上两个磁盘 一主一从
};


#endif