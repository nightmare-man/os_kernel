#ifndef _FS_H_
#define _FS_H_
#define MAX_FILES_PER_PART 4096//每个分区最大文件数，也是inode最大编号+1
#define BITS_PER_SECTOR 4096//每扇区的位数 512*8
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE//一个块就是一个扇区
enum file_types{
	FT_UNKNOW,
	FT_REGULAR,
	FT_DIRECTORY
};

#endif