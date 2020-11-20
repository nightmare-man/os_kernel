#ifndef _FILE_H_
#define _FILE_H_

//file.h中的函数都是 基础的file_table 扇区 inode fd_table dir 的基础操作函数 
#include "../lib/kernel/stdint.h"
#include "./inode.h"
#include "../device/ide.h"

#define MAX_FILE_OPEN 32 //系统最大可打开的文件次数   文件结构表的上限（注意是次数而不是个数 因为一个文件可以打开多次，在文件表里占用多个结构）

struct file{
	uint32_t fd_pos;//文件内操作的位置
	uint32_t fd_flag;
	struct inode* fd_inodes;	
};

enum std_fd{//标准文件描述符 储存在tcb文件描述符表的前三个
	stdin_no,  //0 标准输入
	stdout_no,  //1标准输出
	stderr_no  //2标准错误
};

struct dir{
	struct inode*inode;//inode
	uint32_t dir_pos;// 指向该目录的第n个dir_entry
	uint8_t dir_buf[512];//对inode指向的目录的 数据块的缓存
};
enum bitmap_type{
	INODE_BITMAP,
	BLOCK_BITMAP
};

int32_t get_free_slot_in_global(void);
int32_t pcb_fd_install(int32_t global_fd_idx);
int32_t inode_bitmap_alloc(struct partition* part);
int32_t block_bitmap_alloc(struct partition* part);
void bitmap_sync(struct partition* part,uint32_t bit_idx,uint8_t btmp);
int32_t file_create(struct dir* parent ,char* filename,uint8_t flag);
int32_t file_open(uint32_t inode_no,uint8_t flag);
uint32_t file_close(struct file*file);
int32_t file_write(struct file*file,const void *buf,uint32_t count );
#endif