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
struct inode_position{
	bool two_sec;//是否跨越扇区
	uint32_t sec_lba;//
	uint32_t off_size;//扇区内的偏移
};

//以下函数通过inode_no定位一个inode，保存在传入的ionde_pos里
static void inode_locate(struct partition*part,uint32_t inode_no,struct inode_position*inode_pos){
	ASSERT(inode_no<MAX_FILES_PER_PART);
	uint32_t inode_table_lba=part->sb->inode_table_lba;
	uint32_t inode_size=sizeof(struct inode);
	uint32_t off_size=inode_size*inode_no;
	uint32_t off_sec=off_size/512;
	uint32_t off_size_in_sec=off_size%512;

	uint32_t left_size=512-off_size_in_sec;//该扇区从inode起始位置开始还剩的字节
	if(left_size<inode_size){
		inode_pos->two_sec=true;
	}else{
		inode_pos->two_sec=false;
	}
	inode_pos->sec_lba=part->sb->inode_table_lba+off_sec;
	inode_pos->off_size=off_size_in_sec;
}

//以下函数将inode写到分区
//要求传入buf的指针，而不是在此函数内部malloc buf，因为写入数据到磁盘
//一般是最后一步，如果这一步时才malloc，失败了就得回滚数据状态，（虽然我们在这一步没对inode做啥操作，是memcpy副本，但是保不定主调函数里做了什么处理,需要主调函数自己回滚）
//因此buf要求主调函数先malloc好
void inode_sync(struct partition* part,struct inode* inode,void*io_buf){
	uint8_t inode_no=inode->i_no;
	struct inode_position inode_pos;
	inode_locate(part,inode_no,&inode_pos);
	ASSERT(inode_pos.sec_lba<part->start_lba+part->sec_cnt);
	struct inode temp;
	memcpy(&temp,inode,sizeof(struct inode));
	//这三个数据只在内存中有用，写入磁盘时，清0，因为每次重新从磁盘读inode时必然是全新地打开
	temp.open_cnt=0;
	temp.write_deny=false;
	temp.open_list_elem.next=temp.open_list_elem.prev=NULL;
	
	char*inode_buf=(char*)io_buf;
	if(inode_pos.two_sec){
		ide_read(part->belong_to,inode_pos.sec_lba,inode_buf,2);//读两个扇区
		memcpy((inode_buf+inode_pos.off_size),&temp,sizeof(struct inode));//更新inode所在字节
		ide_write(part->belong_to,inode_pos.sec_lba,inode_buf,2);
	}else{
		ide_read(part->belong_to,inode_pos.sec_lba,inode_buf,1);//只读一个扇区就够了
		memcpy((inode_buf+inode_pos.off_size),&temp,sizeof(struct inode));
		ide_write(part->belong_to,inode_pos.sec_lba,inode_buf,1);
	}
}

//这些函数，将来会作为系统调用被用户进程执行到
//此函数根据i_no 打开一个inode，载入内存（也有可能之前就读取过，返回缓存）
struct inode* inode_open(struct partition*part,uint32_t i_no){
	struct list_elem* i_elem=part->open_inodes.head.next;//先在已经打开的inode列表里找
	struct inode* inode_tar;
	while(i_elem!=&part->open_inodes.tail){//没有到尾，继续遍历
		inode_tar=elem2entry(struct inode,open_list_elem,i_elem);
		if(inode_tar->i_no==i_no){
			inode_tar->open_cnt++;
			return inode_tar;
		}
		i_elem=i_elem->next;
	}
	//如果不在缓存链表里
	//从硬盘读入
	struct inode_position pos;
	inode_locate(part,i_no,&pos);

	/*
		底下这一串操作堪称666
		首先为了实现已经打开的inode结构在内存里的缓存，我们在inode的结构里加入了list_elem 
		然后我们在每个partition里搞了一个链表，来保存他们，是所有线程和进程都共享的，
		partition里的list是内核的全局变量，内核态，但是list_elem inode是malloc的内存，而
		所以要求malloc分配的地址是内核态的，但是用户进程 通过系统调用使用这个函数 malloc还是
		用户态
		因此这里 在sys_malloc之前 强制将cur_thread->pgdir置NULL了，不论是不是内核线程，malloc
		之后再恢复

		进入中断或者调用门 仍是原进程，只有scdedule时才更换cr3 更换esp0 才是切换进程


		另外 共享资源是链表，对链表的访问应该是加锁实现原子的，但是这里只有append 部分内部关中断实现了，
		前面比较i_no时没有，因此不太稳妥，后面继续看吧，多线程这块比较麻烦
	*/
	struct task_struct*cur_thread=running_thread();
	uint32_t* cur_pgdir_bake=cur_thread->pgdir;
	cur_thread->pgdir=NULL;
	inode_tar=(struct inode*)sys_malloc(sizeof(struct inode));
	cur_thread->pgdir=cur_pgdir_bake;
	

	char* inode_buf;
	if(pos.two_sec){
		inode_buf=(char*)sys_malloc(1024);//两个扇区
		ide_read(part->belong_to,pos.sec_lba,inode_buf,2);
	}else{
		inode_buf=(char*)sys_malloc(512);
		ide_read(part->belong_to,pos.sec_lba,inode_buf,1);
	}


	memcpy(inode_tar,inode_buf+pos.off_size,sizeof(struct inode));
	

	list_push(&part->open_inodes,&inode_tar->open_list_elem);//放在最前面而不是append 因为根据 局部性原理 ，最近可能回经常用到
	inode_tar->open_cnt=1;

	cur_thread->pgdir=NULL;
	sys_free(inode_buf);
	cur_thread->pgdir=cur_pgdir_bake;
	return inode_tar;
}
//以下函数关闭inode或者减少inode的打开数
void inode_close(struct inode*i_node){
	enum intr_status old=intr_disable();//链表是公共资源 任何访问都应该互斥
	if(--i_node->open_cnt==0){
		list_remove(&i_node->open_list_elem);
		struct task_struct*cur_thread=running_thread();
		uint32_t* cur_pgdir_bake=cur_thread->pgdir;
		sys_free(i_node);
		cur_thread->pgdir=cur_pgdir_bake;
	}
	intr_set_status(old);
}

void inode_init(uint32_t inode_no,struct inode* new_inode){
	new_inode->i_no=inode_no;
	new_inode->i_size=0;
	new_inode->open_cnt=0;
	new_inode->write_deny=false;

	uint8_t sec_idx=0;
	while(sec_idx<13){
		new_inode->i_blocks[sec_idx]=0;
		sec_idx++;
	}
}