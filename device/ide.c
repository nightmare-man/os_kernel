#include "./ide.h"
#include "../lib/kernel/bitmap.h"
#include "../lib/kernel/list.h"
#include "../thread/sync.h"
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
	bool semaphore disk_done;
	struct disk devices[2];//一个通道上两个磁盘 一主一从
}

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
