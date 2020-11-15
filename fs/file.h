#ifndef _FILE_H_
#define _FILE_H_
#include "../lib/kernel/stdint.h"
#include "./inode.h"
#include "../device/ide.h"

struct file{
	uint32_t fd_pos;//文件内操作的位置
	uint32_t fd_flag;
	struct inode* fd_inodes;	
};

enum std_fd{
	stdin_no,  //0 标准输入
	stdout_no,  //1标准输出
	stderr_no  //2标准错误
};

enum bitmap_type{
	INODE_BITMAP,
	BLOCK_BITMAP
};

#define MAX_FILE_OPEN 32 //系统最大可打开的文件次数   文件结构表的上限（注意是次数而不是个数 因为一个文件可以打开多次，在文件表里占用多个结构）
#endif