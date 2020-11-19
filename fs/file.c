#include "./file.h"
#include "./inode.h"
#include "./fs.h"
#include "./dir.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/stdio_kernel.h"
#include "../thread/thread.h"
#include "../device/ide.h"
#include "../lib/string.h"
#include "../lib/kernel/interrupt.h"
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
			cur->fd_table[local_fd_idx]=global_fd_idx;//安装
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

int32_t file_open(uint32_t inode_no,uint8_t flag){
	int fd_idx=get_free_slot_in_global();
	if(fd_idx==-1){
		printfk("exceed max open files!\n");
		return -1;
	}
	file_table[fd_idx].fd_inodes=inode_open(cur_part,inode_no);
	file_table[fd_idx].fd_flag=flag;
	file_table[fd_idx].fd_pos=0;//每次打开文件 初始化偏移为0
	bool*write_deny=&file_table[fd_idx].fd_inodes->write_deny;//用指针是为了修改
	if(flag &O_RDWR||flag & O_RONLY){
		//如果是写文件 判断是否有其他进程正在写此文件
		//为了互斥 关闭中断后再访问
		enum intr_status old=intr_disable();
		if(*write_deny==false){//如果不是关中断 防止互斥下判断，那就没意义，比如if的时候满足跳进，刚执行完if后被换下，其他线程写这个文件，*write_deny
		//实际上是true了，两个线程都在试图写
			*write_deny=true;//置为true 以后其他线程在此线程没释放之前，就不能写这个文件了
			intr_set_status(old);
		}else{
			intr_set_status(old);
			printfk("file can't be written now!\n");
			return -1;
		}


	}
	
	return pcb_fd_install(fd_idx);//从全局file_table中安装到pcb的文件描述符表，返回下标
}
uint32_t file_close(struct file*file){
	if(file==NULL){
		return -1;
	}
	file->fd_inodes->write_deny=false;//释放写占用
	inode_close(file->fd_inodes);//关闭inodes
	file->fd_inodes=NULL;//释放file_table中的占用
	return 0;	
}
int32_t file_write(struct file*file,const void *buf,uint32_t count ){
	if(file->fd_inodes->i_size+count>BLOCK_SIZE*140){//如果超过了最大值
		printfk("exceed max file_size 71680 bytes,write file failed\n");
		return -1;
	}
	//作者又又又来bug了，这里原文分配的是512 而我分配的是1024
	uint8_t *io_buf=(uint8_t*)sys_malloc(BLOCK_SIZE*2);//为什么是1024而不是512？因为后面要调用inode_sync传给它，需要1024字节的buf
	if(io_buf==NULL){
		printfk("[file.c]file_write malloc memory fail!\n");
		return -1;
	}
	uint32_t* all_blocks=(uint32_t*)sys_malloc(140*sizeof(uint32_t));
	if(all_blocks==NULL){
		printfk("[file.c]file_write malloc memory fail\n");
		return -1;
	}
	const uint8_t* src=buf;
	uint32_t bytes_written=0;//已经写入的字节
	uint32_t size_left=count;//剩余的字节
	uint32_t block_lba=-1;
	uint32_t block_bitmap_idx=0;

	uint32_t sec_idx;
	uint32_t sec_lba;
	uint32_t sec_off_bytes;//扇区内字节的偏移量
	uint32_t sec_left_bytes;
	uint32_t chunk_size;//每次写入硬盘的数据块大小
	int32_t indirect_block_table;//间接表的地址lba
	uint32_t block_idx;
	if(file->fd_inodes->i_blocks[0]==0){//文件全空
		block_lba=block_bitmap_alloc(cur_part);
		if(block_lba==-1){
			printfk("[file.c]file_write malloc block fail\n");
			return -1;
		}
		file->fd_inodes->i_blocks[0]=block_lba;
		block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
		ASSERT(block_bitmap_idx!=0);
		bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);//立刻同步到磁盘

	}
	uint32_t file_has_used_blocks=DIV_ROUND_UP(file->fd_inodes->i_size,BLOCK_SIZE);
	uint32_t file_will_use_blocks=DIV_ROUND_UP((file->fd_inodes->i_size+count),BLOCK_SIZE);
	ASSERT(file_will_use_blocks<=140);

	uint32_t add_blocks=file_will_use_blocks-file_has_used_blocks;

	//以下将所有要操作的block的lba地址写入 all_blocks
	//更新了文件的inode，最后要inode_sync
	//对于间接块 还要写到间接地址表里(硬盘上)
	
	if(add_blocks==0){//块够用，不需要增加新的数据块,写到已有的最后一块
		if(file_will_use_blocks<=12){//待写入数据在直接块
			block_idx=file_will_use_blocks-1;
			all_blocks[block_idx]=file->fd_inodes->i_blocks[block_idx];
		}else{
			ASSERT(file->fd_inodes->i_blocks[12]!=0);
			indirect_block_table=file->fd_inodes->i_blocks[12];
			ide_read(cur_part->belong_to,indirect_block_table,all_blocks+12,1);//将间接块的地址读到all_blocks对应位置
		}
	}else{//需要增加新的数据块，（但是我们仍然要注意已有块的最后一块，可能有空余空间，为了保险起见，我们把这一块的地址也放入all_blocks）
		if(file_will_use_blocks<=12){//还是用直接块
			block_idx=file_has_used_blocks-1;
			all_blocks[block_idx]=file->fd_inodes->i_blocks[block_idx];

			block_idx++;
			while(block_idx<file_will_use_blocks){
				block_lba=block_bitmap_alloc(cur_part);
				if(block_lba==-1){
					
					printfk("[file.c]file_write malloc block fail,situation 1\n");
					return -1;
					
				}
				ASSERT(file->fd_inodes->i_blocks[block_idx]==0);//如果不为0，说明有该块已经被分配，但是未被使用，这是不允许的
				file->fd_inodes->i_blocks[block_idx]=all_blocks[block_idx]=block_lba;
				block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
				bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
				block_idx++;
			}

		}else if(file_has_used_blocks<=12&&file_will_use_blocks>12){//原来是直接块，现在需要使用间接块
			block_idx=file_has_used_blocks-1;
			all_blocks[block_idx]=file->fd_inodes->i_blocks[block_idx];//记录最后一个块
			

			//分配间接表的块
			block_lba=block_bitmap_alloc(cur_part);
			if(block_lba==-1){
				printfk("[file.c]file_write malloc block fail,situation 2\n");
				return -1;
			}
			ASSERT(file->fd_inodes->i_blocks[12]==0);//没有间接表
			indirect_block_table=file->fd_inodes->i_blocks[12]=block_lba;
			block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
			//作者这里掉了一个bitmap_sync，我给补上了 bug+1
			bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

			block_idx=file_has_used_blocks;//第一个待分配块的索引
			while(block_idx<file_will_use_blocks){
				block_lba=block_bitmap_alloc(cur_part);
				if(block_lba==-1){
					printfk("[file.c]file_write malloc block fail,situation 2\n");
					return -1;
				}
				if(block_idx<12){
					ASSERT(file->fd_inodes->i_blocks[block_idx]==0);//确保之前没有分配
					file->fd_inodes->i_blocks[block_idx]=all_blocks[block_idx]=block_lba;
				}else{
					//要写到间接地址表的块中，我们先写到all_blocks中 然后一起ide_write到间接地址表的块里
					all_blocks[block_idx]=block_lba;
				}
				block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
				bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
				block_idx++;
			}
			ide_write(cur_part->belong_to,indirect_block_table,all_blocks+12,1);//同步间接地址表块
		}else if(file_has_used_blocks>12){
			//原本就已经用到了间接块
			ASSERT(file->fd_inodes->i_blocks[12]!=0);
			indirect_block_table=file->fd_inodes->i_blocks[12];
			ide_read(cur_part->belong_to,indirect_block_table,all_blocks+12);//读入间接地址表的 地址项

			block_idx=file_has_used_blocks;//待分配的第一个块的索引
			while(block_idx<file_will_use_blocks){
				block_lba=block_bitmap_alloc(cur_part);
				if(block_lba==-1){
					printfk("[file.c]file_write malloc block fali,situation 3\n");
					return -1;
				}
				all_blocks[block_idx]=block_lba;
				block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
				bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
				block_idx++;
			}
			ide_write(cur_part->belong_to,indirect_block_table,all_blocks+12,1);//同步间接地址表
		}
	}
	//至此 所有待操作的数据块的lba都按照他们对应的索引放在 all_blocks中了， 上面的情况3比较特别，把所有间接地址块都读入了，而其他的情况都是记录从
	//已经占用的最后一个（可能有剩余空间的那个），到分配的块的最后一个
	
	bool has_remain_space=true;//该块是不是还有剩余空间
	file->fd_pos=file->fd_inodes->i_size-1;//文件指针默认为末尾
	while(bytes_written<count){//逐个扇区/块写入

		
		memset(io_buf,0,BLOCK_SIZE);
		sec_idx=file->fd_inodes->i_size/BLOCK_SIZE;//文件末尾所在的块的索引，我们每次都往末尾写，但是除了第一次 末尾块可能需要同步外，其余都是直接写
		//因为其余块都是我们分配的
		sec_lba=all_blocks[sec_idx];//对应的lba
		sec_off_bytes=file->fd_inodes->i_size%BLOCK_SIZE;//第一个空闲字节在最后一个已经使用的块内的索引
		sec_left_bytes=BLOCK_SIZE-sec_off_bytes;//最后一个已经使用的块还剩的空间
		
		chunk_size=size_left<sec_left_bytes?size_left:sec_left_bytes;//判断此次对该块需要写入多少数据（如果剩余写入不足还能够写入的，就剩余，反之就能够）
		if(has_remain_space){//除了第一次写入数据，需要同步原扇区数据到io_buf,其余都是对新分配的扇区的写入，不需要同步
			ide_read(cur_part->belong_to,sec_lba,io_buf,1);
			has_remain_space=false;
		}
		memcpy(io_buf+sec_off_bytes,src,chunk_size);	
		ide_write(cur_part->belong_to,sec_lba,io_buf,1);//写入数据
		src+=chunk_size;
		file->fd_inodes->i_size+=chunk_size;
		file->fd_pos+=chunk_size;
		bytes_written+=chunk_size;
		size_left-=chunk_size;	
	}
	memset(io_buf,0,BLOCK_SIZE);
	inode_sync(cur_part,file->fd_inodes,io_buf);
	sys_free(all_blocks);
	sys_free(io_buf);
	return bytes_written;
}


