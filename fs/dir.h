#ifndef _DIR_H_
#define _DIR_H_
#include "../lib/kernel/stdint.h"
#include "./file.h"
#include "../device/ide.h"
#include "./fs.h"
#define MAX_FILE_NAME_LEN 16
//临时储存目录的结构， inode*指向已打开文件inode链表中的一个，而dir_pos则是在目录中的偏移位置
//必然是目录项大小的整数倍
// struct dir{
// 	struct inode*inode;//inode
// 	uint32_t dir_pos;// 指向该目录的第n个dir_entry
// 	uint8_t dir_buf[512];//对inode指向的目录的 数据块的缓存
// };
//目录项结构
struct dir_entry{
	char file_name[MAX_FILE_NAME_LEN];
	uint32_t i_no;//对应的inode编号
	enum file_types file_type;
};
void dir_close(struct dir*pdir);
struct dir* dir_open(struct partition* part,uint32_t inode_no);
bool search_dir_entry(struct partition* part,struct dir* pdir,const char* name,struct dir_entry* dir_e);
void create_dir_entry(char* filename,uint32_t inode_no,uint8_t filetype,struct dir_entry* d_e);
bool sync_dir_entry(struct dir* parent_dir,struct dir_entry*p_de,void*io_buf);
void open_root_dir(struct partition*part);
bool delete_dir_entry(struct partition*part,struct dir* pdir,uint32_t inode_no,void*io_buf);
struct dir_entry* dir_read(struct dir* dir);
bool dir_is_empty(struct dir * dir);
int32_t dir_remove(struct dir*parent_dir,struct dir*child_dir);
#endif