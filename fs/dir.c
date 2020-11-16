#include "./dir.h"
#include "../lib/kernel/stdint.h"
#include "../device/ide.h"
#include "./inode.h"
#include "../lib/string.h"
#include "../lib/kernel/memory.h"
#include "./file.h"
#include "./file.h"
#include "./inode.h"
#include "./fs.h"
#include "./dir.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/stdio_kernel.h"
#include "../thread/thread.h"
#include "../device/ide.h"
#include "../lib/string.h"

extern struct partition* cur_part;
struct dir root_dir;// 根目录 根据mount不同分区重写这个结构
//定义在fs.c中 当前mount的分区

//以下函数位打开root 目录 实际上就是在内存里记载根目录的结构信息
void open_root_dir(struct partition*part){

	root_dir.inode=inode_open(part,part->sb->root_inode_no);
	root_dir.dir_pos=0;
	
}
//以下函数 传入inode_no 打开对应的dir（也就是返回一个内存中的dir结构的指针）
struct dir* dir_open(struct partition* part,uint32_t inode_no){
	struct dir* pdir=(struct dir*)sys_malloc(sizeof(struct dir));
	pdir->inode=inode_open(part,inode_no);
	pdir->dir_pos=0;
	return pdir;
}
//以下函数传入 dir指针，从对应的数据块中遍历目录项，找到name的那个，存入dir_e指针对应的dir_entry结构
bool search_dir_entry(struct partition* part,struct dir* pdir,const char* name,struct dir_entry* dir_e){
	//首先 将dir* 对应的数据块地址都找到
	
	uint32_t max_block_cnt=140;//最多可能有140个块，也就是inode中的i_blocks[] 数组展开后最多140项（12+128），对应140个块
	uint32_t* all_blocks=(uint32_t*)sys_malloc(max_block_cnt*sizeof(uint32_t));//申请一个140的uint32数组保存对应块的地址
	if(all_blocks==NULL){
		printfk("[search_dir_entry]malloc memory fail!\n");
		return false;
	}
	uint32_t block_idx=0;
	while(block_idx<12){
		all_blocks[block_idx]=pdir->inode->i_blocks[block_idx];//前12个块是 直接块地址表
		block_idx++;
	}
	if(pdir->inode->i_blocks[12]!=0){//如果有间接块地址表
		ide_read(part->belong_to,pdir->inode->i_blocks[12],&all_blocks[block_idx],1);//直接从磁盘读入 剩余的地址表到all_blocks数组的剩余部分
	}

	//按照地址 逐个数据块读入
	uint8_t*buf=(uint8_t*)sys_malloc(SECTOR_SIZE);
	struct dir_entry* p_de=(struct dir_entry*)buf;//以dir entry结构的方式访问buf
	uint32_t dir_entry_size=part->sb->dir_entry_size;//我真的醉了 为什么要专门part->sb里内嵌一个dir_entry_size变量呢？
	//这个变量又一直是 sizeof(struct dir_entry)，那这个变量有啥意义呢？ 难道是为了多文件系统做准备？（不同的part的dir_entry_size不同）
	uint32_t dir_entry_cnt=SECTOR_SIZE/dir_entry_size;//每个扇区/块里能保存 目录项结构的 最大个数 （多余的字节不用，简单起见不允许该结构跨扇区）

	block_idx=0;
	while(block_idx<max_block_cnt){//dir的逐个数据块遍历
		if(all_blocks[block_idx]==0){//不对应数据块
			block_idx++;
			continue;
		}
		ide_read(part->belong_to,all_blocks[block_idx],buf,1);
		uint32_t dir_entry_idx=0;//记录目录项的序号
		while(dir_entry_idx<dir_entry_cnt){
			if(!strcmp(p_de[dir_entry_idx].file_name,name)){//如果相同
				memcpy(dir_e,&p_de[dir_entry_idx],dir_entry_size);
				sys_free(buf);
				sys_free(all_blocks);
				return true;
			}
			dir_entry_idx++;
		}
		block_idx++;
		memset(buf,0,SECTOR_SIZE);
	}
	sys_free(buf);
	sys_free(all_blocks);
	return false;
}
void dir_close(struct dir*pdir){
	if(pdir==&root_dir){//根目录一般不关闭 否则 还需要调用open_root_dir
		return ;
	}
	inode_close(pdir->inode);
	sys_free(pdir);
}

void create_dir_entry(char* filename,uint32_t inode_no,uint8_t filetype,struct dir_entry* d_e){
	ASSERT(strlen(filename)<MAX_FILE_NAME_LEN);
	d_e->i_no=inode_no;
	d_e->file_type=filetype;
}

//以下函数传入一个parent dir* 和一个dir_entry
//工作原理是，访问dir->inode->i_blocks表中lba地址对应的数据块
//如果数据块中还能放得下一个dir_entry就写入到该数据块
//如果不行 就看下一个地址表项lba对应的数据块，如果下一个lba为0，那就alloc一个数据块
//（并更新dir->inode->i_blocks地址表（可能需要新建间接地址表来储存））

//此函数只同步了数据块里的目录项 可能会修改父目录的inode 但是不负责同步！！

//感觉作者写的有bug all_blocks只复制了前12个直接地址 ，其余全初始化为0
//那么这个判断if(dir_inode->i_blocks[block_idx]==0) 无法判断简介块
//到底有没有，所以if(block_idx==12)和大于12的操作都是错误的，会把原来的
//数据覆盖

//我修改了作者的代码 改成了判断dir_inode->i_blocks[block_idx]==0 这样才不会对间接块到底有没有判断有误
bool sync_dir_entry(struct dir* parent_dir,struct dir_entry*p_de,void*io_buf){
	struct inode*dir_inode=parent_dir->inode;
	uint32_t dir_size=dir_inode->i_size;//目录文件的总大小
	uint32_t dir_entry_size=cur_part->sb->dir_entry_size;

	ASSERT(dir_size%dir_entry_size==0);//目录大小 是目录项的整数倍

	uint32_t dir_entry_per_sec=(SECTOR_SIZE/dir_entry_size);
	int32_t block_lba=-1;

	uint8_t block_idx=0;
	uint32_t *all_blocks=(uint32_t*)sys_malloc( sizeof(uint32_t)*140);
	while(block_idx<12){ //只复制了 前12个直接地址
		all_blocks[block_idx]=parent_dir->inode->i_blocks[block_idx];
		block_idx++;
	}
	struct dir_entry*dir_e=(struct dir_entry*)io_buf;//将iobuf当作临时缓冲区 存dir_entry
	int32_t block_bitmap_idx=-1;
	block_idx=0;
	while(block_idx<140){//
		block_bitmap_idx=-1;
		if(dir_inode->i_blocks[block_idx]==0){//如果该地址表项为0，说明需要新建一个数据块来储存dir_entry
			block_lba=block_bitmap_alloc(cur_part);
			if(block_lba==-1){
				printfk("[sync_dir_entry]alloc bitmap for block fail!\n");
				return false;
			}
			block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
			ASSERT(block_bitmap_idx!=-1);
			bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

			block_bitmap_idx=-1;
			if(block_idx<12){
				dir_inode->i_blocks[block_idx]=all_blocks[block_idx]=block_lba;//更新inode里的块地址表
			}else if(block_idx==12){
				dir_inode->i_blocks[12]=block_lba;//将上面分配的块作为 间接地址表
				//每次从位图分配一个块 就及时调用bitmap_sync
				block_lba=-1;
				block_lba=block_bitmap_alloc(cur_part);
				if(block_lba==-1){//如果第二个块分配失败，那么就回退，间接地址表也不要了，把block退回去 bitmap 置0
					block_bitmap_idx=dir_inode->i_blocks[12]-cur_part->sb->data_start_lba;
					bitmap_set(&cur_part->block_bitmap,block_bitmap_idx,0);
					dir_inode->i_blocks[12]=0;
					printfk("[sync_dir_entry]alloc bitmap for block fail!\n");
					return false;
				}
				block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
				ASSERT(block_bitmap_idx!=-1);
				bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

				all_blocks[12]=block_lba;//暂时借用all_blocks+12 当作buf（刚好一个字节，all_blocks[12]是起始4字节），往里面写入
				//间接地址表的第0项（刚分配的lba）
				ide_write(cur_part->belong_to,dir_inode->i_blocks[12],all_blocks+12,1);//写入间接地址表的第一个表项（刚分配的lba）
			}else{
				all_blocks[block_idx]=block_lba;
				ide_write(cur_part->belong_to,dir_inode->i_blocks[12],all_blocks+12,1);
			}
			memset(io_buf,0,SECTOR_SIZE);
			memcpy(io_buf,p_de,dir_entry_size);
			ide_write(cur_part->belong_to,all_blocks[block_idx],io_buf,1);
			dir_inode->i_size+=dir_entry_size;
			return true;
		}
		
		//刚开始循环时 all_blocks[block_idx]!=0 对应有数据块，我们看看这些数据块有没有空间放入dir_entry
		ide_read(cur_part->belong_to,all_blocks[block_idx],io_buf,1);
		uint8_t dir_entry_idx=0;
		while(dir_entry_idx<dir_entry_per_sec){
			if((dir_e+dir_entry_idx)->file_type==FT_UNKNOW){
				//dir_entry 初始化和被删除后都置为 FT_UNKNOW 此处为unknow 说明这个位置空
				memcpy(dir_e+dir_entry_idx,p_de,dir_entry_size);
				ide_write(cur_part->belong_to,all_blocks[block_idx],io_buf,1);
				dir_inode->i_size+=dir_entry_size;
				return true;
			}
			dir_entry_idx++;
		}
		block_idx++;
	}
	printfk("directory is full!\n");
	return false;
}