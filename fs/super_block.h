#ifndef _SUPER_BLOCK_H_
#define _SUPER_BLOCK_H_
#include "../lib/kernel/stdint.h"
//仿照ext2的文件系统实现
struct super_block{
	uint32_t maigc; //标识文件系统类型
	uint32_t sec_cnt;//本分区总扇区数
	uint32_t inode_cnt;//本分区 总inode数 也就是最大文件数
	uint32_t part_lba_start;//本分区的起始lba地址

	uint32_t block_bitmap_lba;//块位图的扇区地址
	uint32_t block_bitmap_sects;//块位图占用扇区数

	uint32_t inode_bitmap_lba;//inode位图lba地址
	uint32_t inode_bitmap_sects;

	uint32_t inode_table_lba;//inode 表扇区地址
	uint32_t inode_table_sects;

	uint32_t data_start_lba ;//数据区开始的第一个扇区号（第一个块所在的扇区号）
	uint32_t root_inode_no;//根目录对应的inode编号
	uint32_t dir_entry_size;//目录项大小， 本来文件大小都记载在目录文件里，现在根目录了 只能写在super_block里了
	uint8_t pad[460];//凑足一个扇区
} __attribute__ ((packed)); //不使用对齐 保证大小不变

#endif