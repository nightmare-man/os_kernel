#ifndef _INODE_H_
#define _INODE_H_
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/list.h"
struct inode{
	uint32_t i_no;//inode的编号
	uint32_t i_size;//文件的大小 字节单位
	uint32_t open_cnt;//文件被打开的次数
	bool write_deny;//不允许同时写，不用semaphore 因为sema_down会阻塞自己 不要阻塞，直接返回失败
	uint32_t i_blocks[13];//块的索引，0-11是直接块索引 12是间接块索引，指向一个块，该块里记录还是索引，是13->更多（因为一个块是一扇区
	//512 一个索引4byte 因此上限是12+128=140个块 140*512byte=70kb）
	struct list_elem open_list_elem;//这个elem用于 已打开的文件inode列表
};


#endif