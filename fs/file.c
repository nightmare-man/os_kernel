#include "./file.h"
#include "./inode.h"
#include "./fs.h"
#include "./dir.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/stdio_kernel.h"
#include "../thread/thread.h"
#include "../device/ide.h"
#include "../lib/string.h"

struct file file_table[MAX_FILE_OPEN];//所有进程线程共用的文件结构表
extern struct partition*cur_part;
//inode 是文件数据描述符
//fd是文件操作描述符


//每个进程都有自己的file descriptor数组，这个数组里存的是 全局的file struct 数组某个元素的下标
//因此file descriptor实际上不是一个完整的描述符 而是指向file struct
//同样 file struct 指向一个inode  inode指向 数据块/扇区 

//以下函数从全局文件结构表中找一个空位
int32_t get_free_slot_in_global(void){
	uint32_t fd_idx=3;
	while(fd_idx<MAX_FILE_OPEN){
		if(file_table[fd_idx].fd_inodes==NULL){
			break;
		}
		fd_idx++;
	}
	if(fd_idx==MAX_FILE_OPEN){//即已经满了还试图open就返回错误
		printfk("exceed max open files\n");
		return -1;
	}
	return fd_idx;
}

//此函数将全局文件描述符（文件结构在表里的索引） 安装到tcb里的 描述符表，返回表里的下标（也就是本地文件描述符）
int32_t pcb_fd_install(int32_t global_fd_idx){
	struct task_struct* cur=running_thread();
	uint8_t local_fd_idx=3;
	while(local_fd_idx<MAX_FILES_OPEN_PER_PROC){
		if(cur->fd_table[local_fd_idx]==-1){//-1表示空闲
			cur->fd_table[local_fd_idx]==global_fd_idx;//安装
			break;
		}
		local_fd_idx++;
	}
	if(local_fd_idx==MAX_FILES_OPEN_PER_PROC){
		printfk("exceed max open files per progress\n");
		return -1;
	}
	return local_fd_idx;
}

//以下函数 分配一个空闲的inode（没有对应文件的） 返回下标
//一般调用后 要使用bitmap_sync 同步修改
int32_t inode_bitmap_alloc(struct partition* part){
	int32_t bit_idx=bitmap_scan(&part->inode_bitmap,1);//从inode bitmap中找一个空位
	if(bit_idx==-1){
		return -1;
	}
	bitmap_set(&part->inode_bitmap,bit_idx,1);
	return bit_idx;
}

//以下函数 分配一个空闲的块/扇区 返回lba (而不是bitmap里的下标)
int32_t block_bitmap_alloc(struct partition* part){
	int32_t block_idx=bitmap_scan(&part->block_bitmap,1);
	if(block_idx==-1){
		return -1;
	}
	bitmap_set(&part->block_bitmap,block_idx,1);
	return (part->sb->data_start_lba+block_idx);
}


//以下函数 将内存中bitmap的idx位所在的512字节同步到磁盘上（最小操作单位512字节）
//block bitmap和inode bitmap 与之前内存池的bitmap不同，需要持久化到内存，并且及时同步

void bitmap_sync(struct partition*part,uint32_t bit_idx,uint8_t btmp){
	uint32_t off_sec=bit_idx/BITS_PER_SECTOR;//偏移的扇区
	uint32_t off_size=off_sec*SECTOR_SIZE;//偏移的扇区起始字节 对应的偏移字节数
	uint32_t sec_lba;
	uint8_t* bitmap_off;
	switch(btmp){
		case INODE_BITMAP:
			sec_lba=part->sb->inode_bitmap_lba+off_sec;//在磁盘里的lba
			bitmap_off=part->inode_bitmap.bits+off_size;//bitmap 的idx位所在的512字节 在内存里对应的起始地址
			break;
		case BLOCK_BITMAP:
			sec_lba=part->sb->block_bitmap_lba+off_sec;
			bitmap_off=part->block_bitmap.bits+off_size;
			break;
	}
	ide_write(part->belong_to,sec_lba,bitmap_off,1);
}
int32_t file_create(struct dir* parent,char* filename,uint8_t flag){
	void* io_buf=(void*)sys_malloc(1024);
	if(io_buf==NULL){
		printfk("[file.c]malloc memory fail!\n");
		return -1;
	}
	uint8_t rollback_step=0;

	//1 分配inode
	int32_t inode_no=inode_bitmap_alloc(cur_part);
	if(inode_no==-1){
		printfk("[file.c]malloc inode_bitmap fail!\n");
		return -1;
	}
	struct inode* new_file_inode=(struct inode*)sys_malloc(sizeof(struct inode));
	if(new_file_inode==NULL){//如果此处失败了，那么前面锁alloc的inode_no也不要了，要回滚 goto rollback
		printfk("[file.c]malloc memory fail!\n");
		rollback_step=1;
		goto rollback;
	}
	inode_init(inode_no,new_file_inode);


	//2 从全局文件结构表中找一个空位
	int fd_idx=get_free_slot_in_global();
	if(fd_idx==-1){
		printfk("[file.c]malloc fd_idx fail!\n");
		rollback_step=2;
		goto rollback;
	}
	file_table[fd_idx].fd_inodes=new_file_inode;
	file_table[fd_idx].fd_pos=0;
	file_table[fd_idx].fd_flag=flag;
	file_table[fd_idx].fd_inodes->write_deny=false;

	//3 构建dir_entry写入父目录的数据块
	struct dir_entry new_dir_entry;
	memset(&new_dir_entry,0,sizeof(struct dir_entry));
	create_dir_entry(filename,inode_no,FT_REGULAR,&new_dir_entry);

	if(!sync_dir_entry(parent,&new_dir_entry,io_buf)){
		printfk("[file.c]sync_dir_entry fail!\n");
		rollback_step=3;
		goto rollback;
	}
	memset(io_buf,0,1024);
	inode_sync(cur_part,parent->inode,io_buf);//因为sync_dir_entry 可能会修改父目录的inode 所以要sync parent->inode

	memset(io_buf,0,1024);
	inode_sync(cur_part,new_file_inode,io_buf);//文件的inode其实init后都是0，只有open_cnt为1 要sync以下

	bitmap_sync(cur_part,inode_no,INODE_BITMAP);//inode 分配了要同步
	list_push(&cur_part->open_inodes,&new_file_inode->open_list_elem);//inode 的open_list_elem.prev 和next的修改不sync到硬盘，因为不用持久化
	new_file_inode->open_cnt=1;
	sys_free(io_buf);
	//4 安装到进程文件描述符表
	return pcb_fd_install(fd_idx);//将全局的文件描述符索引 安装到本地
	
	// 回滚表 不同阶段的失败回滚步数不一样
rollback:
	switch (rollback_step)
	{
	case 3:
		memset(&file_table[fd_idx],0,sizeof(struct file));
		
	case 2:
		sys_free(new_file_inode);
	case 1:
		bitmap_set(&cur_part->inode_bitmap,inode_no,0);
		break;
	}
	sys_free(io_buf);
	return -1;
}