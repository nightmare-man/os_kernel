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
	O_RONLY,  //读 0
	O_WONLY,  //写 1
	O_RDWR,  //读写 10
	O_CREAT=4 //创建 100
};

struct path_search_record{// 搜索的路径记录
	char searched_path[MAX_PATH_LEN]; //已经查找过的路径 比如 /a/b/c/d  如果我们当前正在查找c，那么不论c是否查找到 应该更新为/a/b/c
	struct dir* parent_dir;           //总是指向已查找目录的倒数第二级，如果当前的已查找路径是/a/b/c，那么是b的dir*
	uint8_t file_type;        //file_type已查找目录的最后一级的文件类型，没找到是FT_UNKNOW
};
int32_t path_depth_cnt(char* pathname);
void filesys_init();
int32_t sys_open(const char* pathname,uint8_t flags);
#endif