#include "./dir.h"
#include "../lib/kernel/stdint.h"
#include "../device/ide.h"
#include "./inode.h"
#include "../lib/string.h"
#include "../lib/kernel/memory.h"
struct dir root_dir;// 根目录 根据mount不同分区重写这个结构

//以下函数位打开root 目录 实际上就是在内存里记载根目录的结构信息
void open_root_dir(struct partition*part){
	root_dir.inode=inode_open(part,part->sb->root_inode_no);
	root_dir.dir_pos=0;
}
//以下函数 根据传入的inode_no 返回一个dir* dir*的inode*指向 inode_no对应的
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
	d_e->f_type=filetype;
}