#ifndef _DIR_H_
#define _DIR_H_
#define MAX_FILE_NAME_LEN 16
#include "./inode.h"
#include "../lib/kernel/stdint.h"
#include "./fs.h"
//临时储存目录的结构， inode*指向已打开文件inode链表中的一个，而dir_pos则是在目录中的偏移位置
//必然是目录项大小的整数倍
struct dir{
	struct inode*inode;
	uint32_t dir_pos;//
	uint8_t dir_buf[512];
};
//目录项结构
struct dir_entry{
	char file_name[MAX_FILE_NAME_LEN];
	uint32_t i_no;//对应的inode编号
	enum file_types f_type;
};

#endif