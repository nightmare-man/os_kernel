#ifndef _FS_H_
#define _FS_H_
#include "../lib/kernel/stdint.h"
#include "./file.h"

#define MAX_FILES_PER_PART 4096//每个分区最大文件数，也是inode最大编号+1
#define BITS_PER_SECTOR 4096//每扇区的位数 512*8
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE//一个块就是一个扇区

#define MAX_PATH_LEN 512 //最大路径名长度
enum file_types{
	FT_UNKNOW,
	FT_REGULAR,
	FT_DIRECTORY
};

enum oflags{
	O_RONLY,  //读
	O_WONLY,  //写
	O_RDWR,  //读写
	O_CREAT=4 //创建
};

struct path_search_record{// 搜索的路径记录
	char searched_path[MAX_PATH_LEN]; //已经查找过的路径 比如 /a/b/c/d 我们按顺序 先找a 不论找到与否 /a是searched_path,如果找不到 就结束 找到就找/a/b //同样更新 searched_path /a/b
	struct dir* parent_dir;           //记录直接父目录 如果searched path 是 "/a/b"   那么直接父目录是a对应的dir*
	uint8_t file_type;        //file_type是  b的type 如果b没找到 那就是FT_UNKNOW
};
int32_t path_depth_cnt(char* pathname);
void filesys_init();
int32_t sys_open(const char* pathname,uint8_t flags);
#endif