#ifndef _INODE_H_
#define _INODE_H_
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"
#include "../device/ide.h"
#include "./inode.h"
struct inode{
	uint32_t i_no;//inode的编号
	uint32_t i_size;//文件的大小 字节单位
	uint32_t open_cnt;//文件被打开的次数
	bool write_deny;//不允许同时写，不用semaphore 因为sema_down会阻塞自己 不要阻塞，直接返回失败
	uint32_t i_blocks[13];//块的索引，0-11是直接的块索引 12是间接块索引，指向一个块，该块里记录还是块索引，是13->更多（因为一个块是一扇区
	//512 一个索引4byte 因此上限是12+128=140个块 140*512byte=70kb 文件上限）
	struct list_elem open_list_elem;//inode_stable 存在磁盘上，读一个文件需要先从磁盘读其inode，因此我们构建一个inode 列表，缓存已经读过的inode
};
void inode_sync(struct partition* part,struct inode* inode,void*io_buf);
struct inode* inode_open(struct partition*part,uint32_t i_no);
void inode_close(struct inode*i_node);
void inode_init(uint32_t inode_no,struct inode* new_inode);
#endif