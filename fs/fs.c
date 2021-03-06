#include "./fs.h"
#include "./inode.h"
#include "./dir.h"
#include "./super_block.h"
#include "../device/ide.h"
#include "../lib/kernel/stdio_kernel.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/stdint.h"
#include "../lib/string.h"
#include "../device/ide.h"
#include "../lib/kernel/list.h"
#include "./file.h"
#include "../thread/thread.h"
#include "../device/console.h"
extern uint8_t channel_cnt;//在ide.c中定义 在ide_init里初始化
extern struct ide_channel channels[2];//同样
struct partition*cur_part;//记录当前所选用的分区 ，由mout_parttion负责切换
extern struct list partition_list;// 所有分区的链表，定义在ide.c里，由partition_scan初始化
extern struct dir root_dir;//根目录的 dir结构  由dir.c定义 并由open_root_dir 初始化
extern struct file file_table[MAX_FILE_OPEN];//定义在file.c里 由本文件中filesys_init初始化
static void partition_format(struct disk*hd,struct partition*part){

	uint32_t boot_sector_sects=1;
	uint32_t super_block_sects=1;
	uint32_t inode_bitmap_sects=DIV_ROUND_UP(MAX_FILES_PER_PART,BITS_PER_SECTOR);

	uint32_t inode_table_sects=DIV_ROUND_UP((sizeof(struct inode)*MAX_FILES_PER_PART),SECTOR_SIZE);

	uint32_t used_sects=boot_sector_sects+super_block_sects+inode_bitmap_sects+inode_table_sects;//已经使用的，实际上还没算上block_bitmap_sects,偏小
	uint32_t free_sects=part->sec_cnt-used_sects;//偏大

	uint32_t block_bitmap_sects=DIV_ROUND_UP(free_sects,BITS_PER_SECTOR);//由于free_sects偏大，因此给block_bitmap保留的空间偏大，不会不够用
	uint32_t real_free_sects=free_sects-block_bitmap_sects;
	uint32_t block_bitmap_bit_len=real_free_sects;//实际free_block的数量即实际block_bitmap的len
	block_bitmap_sects=DIV_ROUND_UP(block_bitmap_bit_len,BITS_PER_SECTOR);//更新block_bitmap为实际占用的扇区
	
	//1 初始化super_block并写入磁盘
	struct super_block sb;
	sb.magic=0x19980114;//文件系统类型
	sb.sec_cnt=part->sec_cnt;
	sb.inode_cnt=MAX_FILES_PER_PART;
	sb.part_lba_start=part->start_lba;
	
	sb.block_bitmap_lba=sb.part_lba_start+2;//第0是obr，第1是super_block 第2是block bitmap 3 inode bitmap 4 inode tab 5 root dir(第0个数据块)
	sb.block_bitmap_sects=block_bitmap_sects;

	sb.inode_bitmap_lba=sb.block_bitmap_lba+sb.block_bitmap_sects;
	sb.inode_bitmap_sects=inode_bitmap_sects;

	sb.inode_table_lba=sb.inode_bitmap_lba+sb.inode_bitmap_sects;
	sb.inode_table_sects=inode_table_sects;

	sb.data_start_lba=sb.inode_table_lba+sb.inode_table_sects;
	sb.root_inode_no=0;//inode_table[0]为root_dir的inode
	sb.dir_entry_size=sizeof(struct dir_entry);

	printfk("%s info:\n",part->name);
	printfk("  magic:0x%x\n  part_lba_base:0x%x\n  all sectors:0x%x\n  inode_cnt:0x%x\n  block_bitmap_lba:0x%x\n\
	block_bitmap_sectors:0x%x\n  inode_bitmap_lba:0x%x\n  inode_bitmap_sectors:0x%x\n  inode_table_lba:0x%x\n  inode_table_sectors:0x%x\n\
	 data_lba:0x%x\n  data_sectors:0x%x\n",sb.magic,sb.part_lba_start,sb.sec_cnt,sb.inode_cnt,sb.block_bitmap_lba,sb.block_bitmap_sects,\
	sb.inode_bitmap_lba,sb.inode_bitmap_sects,sb.inode_table_lba,sb.inode_table_sects,sb.data_start_lba,real_free_sects);

	//struct disk*hd=part->belong_to;//不用新建一个disk结构，而是直接用指针，实体在ide.c里的有（struct ide_channel数组channels  而channel里有disk数组deives[2]）
	//这句作者写错了？已经传入了disk*啊
	
	ide_write(hd,part->start_lba+1,&sb,1);//写入超级块
	printfk("super_block_lba:0x%x\n",part->start_lba+1);

	//构造一个足够大的缓冲区，用来储存这block_bitmap inode_bitmap inode_table ，因为格式化时先要在内存里构造他们，再把他们写到磁盘里
	//而以后使用file_system时则直接从磁盘读入内存
	uint32_t buf_size=(sb.block_bitmap_sects>sb.inode_bitmap_sects?sb.block_bitmap_sects:sb.inode_bitmap_sects);
	buf_size=(buf_size>sb.inode_table_sects?buf_size:sb.inode_table_sects);
	buf_size*=SECTOR_SIZE;

	uint8_t* buf=(uint8_t*)sys_malloc(buf_size);

	//2 初始化 block_bitmap 并写入磁盘
	buf[0]=0x01; //根目录已经占用了第0个块
	uint32_t block_bitmap_last_byte=block_bitmap_bit_len/8;// block_bitmap最后一个字节的索引
	uint32_t block_bitmap_last_bit=block_bitmap_bit_len%8;//block_bitmap最后一字节中第一个超过范围的bit的索引

	//  [       扇区1            ][... [字节n  ][字节n+1   bitmap_bit_len |     ]          扇区2            ]
	uint32_t last_size=(BLOCK_SIZE-(block_bitmap_last_byte%SECTOR_SIZE));//last_size是bitmap中最后一扇区中  不满1字节的 剩余字节数目
	//即从扇区2的字节n+1到511的数目,这部分字节的bit 不对应 free_block 不可用，应该置为1，但是注意 字节n+1中有一部分对应，应该置0

	memset(&buf[block_bitmap_last_byte],0xff,last_size);//先全部置1

	uint8_t bit_idx;
	for(bit_idx=0;bit_idx<block_bitmap_last_bit;bit_idx++){
		buf[block_bitmap_last_byte] &= ~(1<<bit_idx);//~取反 某一位置0 用该位&0 其余&1
	}
	ide_write(hd,sb.block_bitmap_lba,buf,sb.block_bitmap_sects);

	//3 初始化 inode_bitmap 并写入磁盘
	memset(buf,0,buf_size);
	buf[0]=0x01;//根目录占用第一个inode
	//由于我们规定最大文件数4096 因此inode位图bit也是4096bit刚好一个扇区，因此bitmap没有不完整扇区部分
	ide_write(hd,sb.inode_bitmap_lba,buf,sb.inode_bitmap_sects);

	//4 初始化inode_table并写入(inode_table[0]是根目录对应的inode)
	memset(buf,0,buf_size);
	struct inode* root_dir_inode=(struct inode*)buf;
	root_dir_inode->i_no=0;
	root_dir_inode->i_size=sb.dir_entry_size*2;//初始化就两个目录项 "./ " "../"
	root_dir_inode->i_blocks[0]=sb.data_start_lba;//root_dir 在第0个数据块
	ide_write(hd,sb.inode_table_lba,buf,sb.inode_table_sects);

	//5 初始化第0个数据块(根目录) 并写入

	memset(buf,0,buf_size);
	struct dir_entry* p_de=(struct dir_entry*)buf;
	memcpy(p_de,".",1);//指向当前目录
	p_de->i_no=0;//不论是 . 还是 .. 都是根目录 inode编号都是0
	p_de->file_type=FT_DIRECTORY;

	p_de++;//下一个目录项指针位置

	memcpy(p_de,"..",2);
	p_de->i_no=0;
	p_de->file_type=FT_DIRECTORY;

	ide_write(hd,sb.data_start_lba,buf,1);

	printfk("  root_dir_lba:0x%x\n",sb.data_start_lba);
	printfk("%s format done\n",part->name);
	sys_free(buf);
}

//sb_buf 以及其他的几个malloc的内存都没有free，可能是后面还要用？
//但是保存分配的地址都是局部变量，出函数就内存泄漏了啊
//mark以下

//以下函数接收list_elem 检查对应的partition->name和传入的参数是否一致，
//如果一致就mout该partition，用作traversal函数的参数实现指定分区的mount

//mount的本质 是加载该分区的 super_block  block bitmap inode bitmap 
//并初始化 该分区的open_inodes队列
//不立即加载inode table 因为特别大 没必要一次加载到内存，可以需要时再加载

//这些part都在device/ide.c 的partition_scan 时创建然后在内存中一直保存的，
// 因此我们mount_partiiton后的part->sb part->block_bitmap_bits malloc后也一直保存在内存
//除非unmount,所以我们这里没有 sys_free
//而sb_buf作为缓冲区作者也没free？ 又是bug

static bool mount_partition(struct list_elem*pelem,int32_t arg){
	char*part_name=(char*)arg;
	struct partition* part=elem2entry(struct partition,part_tag,pelem);
	if(!strcmp(part_name,part->name)){//如果返回0 说明相同
		cur_part=part;//更新cur_part
		struct disk*hd=cur_part->belong_to;


		//1 加载super_block
		struct super_block* sb_buf=(struct super_block*)sys_malloc( SECTOR_SIZE );//缓冲区
		//应该为扇区大小的整数倍
		cur_part->sb=(struct super_block*)sys_malloc(sizeof(struct super_block));
		if(cur_part->sb==NULL){
			PANIC("  alloc memory faild\n");
		}
		memset(sb_buf,0,1);
		ide_read(hd,cur_part->start_lba+1,sb_buf,1);
		memcpy(cur_part->sb,sb_buf,sizeof(struct super_block));


		//2 加载block_bitmap		
		cur_part->block_bitmap.bits=(uint8_t*)sys_malloc(sb_buf->block_bitmap_sects*SECTOR_SIZE);
		if(cur_part->block_bitmap.bits==NULL){
			PANIC("  alloc memory faild\n");
		}
		cur_part->block_bitmap.btmp_bytes_len=sb_buf->block_bitmap_sects*SECTOR_SIZE;//有一部分不对应资源 但是没关系，我们早就置1了
		ide_read(hd,sb_buf->block_bitmap_lba,cur_part->block_bitmap.bits,sb_buf->block_bitmap_sects);

		//3 加载inode位图
		cur_part->inode_bitmap.bits=(uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects*SECTOR_SIZE);
		if(cur_part->inode_bitmap.bits==NULL){
			PANIC("  alloc memory faild\n");
		}
		cur_part->inode_bitmap.btmp_bytes_len=sb_buf->inode_bitmap_sects*SECTOR_SIZE;
		ide_read(hd,sb_buf->inode_bitmap_lba,cur_part->inode_bitmap.bits,sb_buf->inode_bitmap_sects);


		//这里没有在mount时就读入inode_table 因为和对应资源的位图不同，位图需要及时更新，因此需要第一时间读入内存，然后修改后立马写入
		//而inode_table则太大了，且里面是记载文件，我们现在只挂载 不操作文件，因此没有读入内存

		//4 list_init
		list_init(&cur_part->open_inodes);
		printfk("mount %s done!\n",cur_part->name);
		sys_free(sb_buf);//我自己添加的free
		
	}
	return false;//ret false是为list_traversal继续遍历的要求
}

void filesys_init(){
	uint8_t channel_no=0,dev_no=0,part_idx=0;
	struct super_block* sb_buf=(struct super_block*)sys_malloc(sizeof(struct super_block));
	if(sb_buf==NULL){
		PANIC("alloc memory failed!!\n");
	}
	printfk("searching filesystem......\n");
	while(channel_no<channel_cnt){
		dev_no=0;
		while(dev_no<2){
			if(channel_no==0&&dev_no==0){//跳过ata0_master 裸盘
				dev_no++;
				continue;
			}
			struct disk*hd=&channels[channel_no].devices[dev_no];
			struct partition*part=hd->prim_parts;
			while(part_idx<12){
				if(part_idx==4){//到第五个分区时为逻辑分区
					part=hd->logic_parts;
				}
				if(part->sec_cnt!=0){//分区存在（channels作为全局变量定义的，因此里面内嵌的disk及partition结构都默认为NULL，
				//这样判断理论上是没问题的，不过 自己没有memeset初始化的结构还是不要这么判断比较好）
					memset(sb_buf,0,SECTOR_SIZE);
					ide_read(hd,part->start_lba+1,sb_buf,1);
					if(sb_buf->magic==0x19980114){
						printfk("%s has filesystem\n",part->name);
					}else{//其他文件系统都不支持 当作没有文件系统 format
						printfk("formatting %s's partition %s......",hd->name,part->name);
						partition_format(hd,part);
					}

				}
				part_idx++;
				part++;
			}
			dev_no++;
		}
		channel_no++;
	}
	sys_free(sb_buf);
	char default_part[8]="sdb1";
	list_traversal(&partition_list,mount_partition,(int32_t)default_part);
	
	
	open_root_dir(cur_part);

	uint32_t fd_idx=0;
	while (fd_idx<MAX_FILE_OPEN)
	{
		file_table[fd_idx].fd_inodes=NULL;
		fd_idx++;
	}
	
}

//以下函数将pathname的最上层路径名 存入name_store，并返回指向下一层路径的指针
static char* path_parse(char* pathname,char*name_store){
	char*ptr=pathname;
	if(ptr[0]=='/'){
		while(*(++ptr)=='/');//跳过'/'
	}
	while(*ptr!='/'&&*ptr!=0){
		*name_store=*ptr;
		name_store++;
		ptr++;
	}
	if(pathname[0]==0){
		return NULL;
	}
	//printfk("after parse path:%s\n",ptr);
	return ptr;//返回指向下一层路径的新指针
}
//以下函数返回路径深度
int32_t path_depth_cnt(char* pathname){
	ASSERT(pathname!=NULL);
	char*p=pathname;
	char name[MAX_FILE_NAME_LEN];
	uint32_t path_depth=0;
	p=path_parse(p,name);
	while(name[0]){
		path_depth++;
		memset(name,0,MAX_FILE_NAME_LEN);
		if(p){
			p=path_parse(p,name);
		}
	}
	return path_depth;
}


//搜索文件pathname,若找到则返回其inode号，否则返回-1
//！！此函数会调用dir_open 打开parent_dir(也就是分配内存来记录相关信息) 要求主调函数调用后自行调用dir_close，不然会内存泄漏！！
static int search_file(const char* pathname,struct path_search_record*searched_record){

	//如果是查找根目录 直接返回
	if( !strcmp(pathname,"/")  || !strcmp(pathname,"/.") || !strcmp(pathname,"/..")){
		searched_record->file_type=FT_DIRECTORY;
		searched_record->parent_dir=&root_dir;
		searched_record->searched_path[0]=0;//搜索路径空
		return 0;
	}
	uint32_t path_len=strlen(pathname);
	//保证是 '/'开始的路径 
	ASSERT(pathname[0]=='/' && path_len>1 && path_len<MAX_PATH_LEN);

	char* sub_path=(char*)pathname;

	struct dir* parent_dir=&root_dir;
	struct dir_entry dir_e;

	char name[MAX_FILE_NAME_LEN]={0};//记录每级目录的名称 /a/b/c/d 分别为 a b c d
	searched_record->parent_dir=parent_dir;
	searched_record->file_type=FT_UNKNOW ;//file_type是记录每级带查找目录项的类型，初始化为FT_UNKNOW
	//化为FT_UNKNOW
	uint32_t parent_inode_no=0;
	sub_path=path_parse(sub_path,name);

	
	//循环 如果能够解析 下一级目录名
	while(name[0]){

		ASSERT(strlen(searched_record->searched_path)<MAX_PATH_LEN);
		//已经解析了带查找的下一级目录名，因此将查找他，无论是不是存在 都得更新searched_path
		strcat(searched_record->searched_path,"/");
		strcat(searched_record->searched_path,name);

		//查找
		if(search_dir_entry(cur_part,parent_dir,name,&dir_e)){
			//如果找到， 先试图再解析下一级目录项名字

			memset(name,0,MAX_FILE_NAME_LEN);
			if(sub_path){//路径还没完，继续搜索下一级目录名
				sub_path=path_parse(sub_path,name);
			}
			//如果本目录项 类型是目录，就更新paret_inode_no和searched_record->parent_dir(需要open_dir),同时close之前的parent_dir
			if(FT_DIRECTORY==dir_e.file_type){
				parent_inode_no=parent_dir->inode->i_no;
				dir_close(parent_dir);
				parent_dir=dir_open(cur_part,dir_e.i_no);
				searched_record->parent_dir=parent_dir;
				continue;//在找到的目录项为目录继续解析下一级之前，我们的parent_inode_no总是指向searched_path中的倒数第二级目录的
						//而searched_record->parent_dir总是指向倒数第一级的目录的，因此 最后出循环后，才有了329行的代码(务必让parent_dir指向父目录)
			//如果本级目录项 类型是 文件，立即终止解析，并设置file_type 为FT_REGULAR
			}else if(FT_REGULAR==dir_e.file_type){//如果是普通文件 搜索到此停止
				searched_record->file_type=FT_REGULAR;
				return dir_e.i_no;
			}
		}else{
			//如果找不到 目录项 返回-1 但是parent_dir没有关闭， 因为有时候找不到需要创建，因此仍然留着
			return -1; 
		}
	}
	//全是目录的路径，解析完毕后会执行到这里 
	dir_close(searched_record->parent_dir);
	searched_record->parent_dir=dir_open(cur_part,parent_inode_no);
	searched_record->file_type=FT_DIRECTORY;
	return dir_e.i_no;
}

int32_t sys_open(const char* pathname,uint8_t flags){
	if(pathname[strlen(pathname)-1]=='/'){//如果是目录 不支持 此为打开文件
		printfk("[fs.c] sys_open can't open a dir %s\n",pathname);
		return -1;
	}
	ASSERT(flags<7);// 几个open_flags可以用 |组合 最大不能超过7   o_creat|o_rdwr  是最大值 也就是 100|010 =6 因此不能超过7
	int32_t fd =-1;
	struct path_search_record searched_record;
	memset(&searched_record,0,sizeof(struct path_search_record));//从栈中分配的空间，要清0初始化
	uint32_t path_depth=path_depth_cnt((char*)pathname);

	int inode_no=search_file((char*)pathname,&searched_record);
	bool found=inode_no==-1?false:true;
	if(searched_record.file_type==FT_DIRECTORY){
		printfk("[fs.c] sys_open can't open a dir %s\n",(char*)pathname);
		dir_close(searched_record.parent_dir);//search_file 时调用了dir_open() malloc struct dir 大小的内存，在此处必须关闭 防止内存泄漏
		return -1;
	}
	uint32_t searched_path_depth=path_depth_cnt(searched_record.searched_path);//实际搜索的深度
	if(path_depth!=searched_path_depth){//没有访问到所有路径，中间断了 说明找不到中间的目录项
		printfk("can't access %s:Not a directory,subpath %s is't exist\n",(char*)pathname,searched_record.searched_path);
		dir_close(searched_record.parent_dir);
		return -1;
	}

	if(!found && !( flags&O_CREAT)){//路径相等 但是没有找到 也就是最后一级路径上找不到，但是又不是创建 
		printfk("in path %s,file %s is not exist\n",(char*)pathname,(strrchr(pathname,'/')+1));
		dir_close(searched_record.parent_dir);
		return -1;
	}else if(found && (flags&O_CREAT)){//如果文件已经存在 但是要创建
		printfk("%s has already existed\n",(char*)pathname);
		dir_close(searched_record.parent_dir);
		return -1;
	}
	switch(flags&O_CREAT){
		case O_CREAT://
			printfk("creating file\n");
			fd=file_create(searched_record.parent_dir,(strrchr(pathname,'/')+1),flags);
			dir_close(searched_record.parent_dir);
			break;
		default: //其余情况均需要打开文件
			fd=file_open(inode_no,flags);//先打开文件
	
	}
	printfk("cur_part is %s ,data block start:0x%x\n",cur_part->name,cur_part->sb->data_start_lba);
	printfk("inode_bitmap start:0x%x\n",cur_part->sb->inode_bitmap_lba);
	printfk("block_bitmap start:0x%x\n",cur_part->sb->block_bitmap_lba);
	printfk("inode_table start:0x%x   inode size:0x%x\n",cur_part->sb->inode_table_lba,sizeof(struct inode));
	
	return fd;//返回本地文件描述符  此fd是任务tcb->fd_table数组中元素的下标

}
uint32_t fd_local2global(uint32_t local_fd){
	struct task_struct *cur=running_thread();
	uint32_t global_fd=cur->fd_table[local_fd];
	ASSERT(global_fd>0 && global_fd<MAX_FILE_OPEN);
	return global_fd;
}
int32_t sys_close(uint32_t local_fd){
	int32_t ret=-1;
	if(local_fd>2){
		int32_t gloabl_fd=fd_local2global(local_fd);
		ret=file_close(&file_table[gloabl_fd]);
		running_thread()->fd_table[local_fd]=-1;//释放pcb中对fd_table的占用;
	}
	return ret;
}
int32_t sys_write(int32_t fd,const void* buf,uint32_t count){//传入的是 文件描述符 而不是全局的file*偏移
	if(fd<0){
		printfk("[fs.c]sys_write fd error\n");
		return -1;
	}
	if(fd==stdout_no){
		char temp_buf[1024]={0};
		memcpy(temp_buf,buf,count);
		console_put_str(temp_buf);//没有再使用printfk 提高效率？？？ //ok我知道了 因为printf以及printfk都是调用的write（旧版），而write用的sys_write
		return count;             //防止循环调用 但是printfk直接用的是console_put_str 实际上旧版的sys_write也是console_put_str
	}
	uint32_t _fd=fd_local2global(fd);//
	struct file* wr_file=&file_table[_fd];
	if(wr_file->fd_flag&O_WONLY||wr_file->fd_flag&O_RDWR){//权限检查 inode->write_deny是打开文件时检查，而这个是写入文件是检查
		uint32_t byte_written=file_write(wr_file,buf,count);
		return byte_written;
	}else{
		console_put_str("[fs.c]sys_write:not allowed to write file without flag O_WRONLY or O_RDWR\n");
		return -1;
	}
}
int32_t sys_read(int32_t fd,void* buf,uint32_t count){
	if(fd<0){
		printfk("[fs.c]sys_read fd can't be negative\n");
		return -1;
	}
	uint32_t _fd=fd_local2global(fd);
	struct file*r_file=&file_table[_fd];
	if(r_file->fd_flag&O_WONLY){
		printfk("[fs.c]sys_read:not allowed to read file with flag O_WRONLY\n");
		return -1;
	}
	return file_read(r_file,buf,count);
}
int32_t sys_lseek(int32_t fd,int32_t offset,uint8_t whence){
	if(fd<0){
		printfk("[fs.c]sys_lseek:fd error\n");
		return -1;
	}
	ASSERT(whence>0&&whence<3);
	uint32_t _fd=fd_local2global(fd);
	struct file* pf=&file_table[_fd];
	int32_t new_pos=0;
	int32_t file_size=(int32_t)pf->fd_inodes->i_size;
	switch (whence)
	{
	case SEEK_SET:
		new_pos=offset;
		break;
	case SEEK_CUR:
		new_pos=(int32_t)pf->fd_pos+offset;
		break;
	case SEEK_END:
		new_pos=file_size+offset;
		break;
	}
	if(new_pos<0||new_pos>(file_size-1)){//读一个文件 读完后pos能指向 file_size的索引，也就是最后以字节的下一个，
											//但是设置时就不能，所以=file_size就fail了
		return -1;
	}
	pf->fd_pos=new_pos;
	return pf->fd_pos;
}

//以下函数删除文件 非目录
int32_t sys_unlink(const char* pathname){
	ASSERT(strlen(pathname)<MAX_PATH_LEN);
	struct path_search_record searched_record;
	memset(&searched_record,0,sizeof(searched_record));
	int inode_no=search_file(pathname,&searched_record);
	ASSERT(inode_no!=0);//当然不会是根目录的inode_no啦
	if(inode_no==-1){
		printfk("file %s not found!\n",pathname);
		dir_close(searched_record.parent_dir);
		return -1;
	}
	if(searched_record.file_type==FT_DIRECTORY){
		printfk("can't delete a directory with unlink(),use rmdir() to instead\n");
		dir_close(searched_record.parent_dir);
		return -1;
	}

	//检查是不是在使用中 （也就是在pcb 中fd_table 全局 file_table inode_open队列中，只有sys_close后才可以删除）
	uint32_t file_idx=0;
	while(file_idx<MAX_FILE_OPEN){
		if( file_table[file_idx].fd_inodes!=NULL && (uint32_t)inode_no==file_table[file_idx].fd_inodes->i_no ){
			break;//找到了
		}
		file_idx++;
	}
	if(file_idx<MAX_FILE_OPEN){
		printfk("file %s is in use,not allow to delete!\n",pathname);
		dir_close(searched_record.parent_dir);
		return -1;
	}
	
	void*io_buf=sys_malloc(SECTOR_SIZE*2);
	if(io_buf==NULL){
		printfk("[fs.c]sys_unlink:malloc memory fail\n");
		dir_close(searched_record.parent_dir);
		return -1;
	}
	struct dir* parent_dir=searched_record.parent_dir;
	delete_dir_entry(cur_part,parent_dir,inode_no,io_buf);//先删除父目录中对应的目录项
	inode_release(cur_part,inode_no);
	sys_free(io_buf);
	dir_close(searched_record.parent_dir);
	return 0;
}
int32_t sys_mkdir(const char*pathname){
	uint8_t rollback_step=0;
	void* io_buf=sys_malloc(SECTOR_SIZE*2);
	if(io_buf==NULL){
		printfk("[fs.c]sys_mkdir:malloc memory fail\n");
		return -1;
	}

	struct path_search_record searched_record;
	memset(&searched_record,0,sizeof(searched_record));
	int inode_no=-1;
	inode_no=search_file(pathname,&searched_record);
	if(inode_no!=-1){//如果找到了同名目录/文件 那就报错
		printfk("[fs.c]sys_mkdir:file or directory %s exist!\n",pathname);
		rollback_step=1;
		goto rollback;
	}
	//如果没找到 也要看是同名文件没找到还是中间目录不存在
	uint32_t pathname_depth=path_depth_cnt((char*)pathname);
	uint32_t path_searched_depth=path_depth_cnt(searched_record.searched_path);
	if(pathname_depth!=path_searched_depth){//说明中间目录不存在
		printfk("[fs.c]sys_mkdir:cannot access %s:Not a directory,subpath %s does not exist!\n",pathname,searched_record.searched_path);
		rollback_step=1;
		goto rollback;
	}
	//至此 确定父目录存在且不存在同名
	struct dir* parent_dir=searched_record.parent_dir;
	char*dirname=strrchr( searched_record.searched_path,'/')+1;//在这里searched_record.searched_path和pathname不一定相同，
	//因为pathname可能以/结尾 ，而前者会去掉结尾的/
	inode_no=inode_bitmap_alloc(cur_part);
	if(inode_no==-1){
		printfk("[fs.c]sys_mkdir:malloc inode_bitmap fail\n");
		rollback_step=1;
		goto rollback;
	}
	//立即同步占用
	bitmap_sync(cur_part,inode_no,INODE_BITMAP);
	struct inode new_dir_inode;
	inode_init(inode_no,&new_dir_inode);
	uint32_t block_bitmap_idx=0;
	uint32_t block_lba=-1;
	block_lba=block_bitmap_alloc(cur_part);
		
	if(block_lba==-1){
		rollback_step=2;//前面有几个需要回滚的操作 rollback_step就为几
		goto rollback;
	}
	new_dir_inode.i_blocks[0]=block_lba;
	block_bitmap_idx=block_lba-cur_part->sb->data_start_lba;
	bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
	
	memset(io_buf,0,SECTOR_SIZE*2);
	struct dir_entry* pde=(struct dir_entry*)io_buf;
	memcpy(pde->file_name,".",1);// 创建. ..目录项 分别位于0 1
	pde->file_type=FT_DIRECTORY;
	pde->i_no=inode_no;//指向自己

	pde++;
	memcpy(pde->file_name,"..",2);
	pde->file_type=FT_DIRECTORY;
	pde->i_no=parent_dir->inode->i_no;//指向父目录
	//同步到硬盘
	ide_write(cur_part->belong_to,block_lba,io_buf,1);

	new_dir_inode.i_size=2*sizeof(struct dir_entry);

	//在父目录里添加自己的目录项
	struct dir_entry new_dir_entry;
	memset(&new_dir_entry,0,sizeof(struct dir_entry));
	create_dir_entry(dirname,new_dir_inode.i_no,FT_DIRECTORY,&new_dir_entry);
	memset(io_buf,0,SECTOR_SIZE*2);
	if(!sync_dir_entry(parent_dir,&new_dir_entry,io_buf)){//如果写入失败
		printfk("[fs.c]sys_mkdir:sync_dir_entry to dsik fail\n");
		rollback_step=3;
		goto rollback;
	}
	
	memset(io_buf,0,SECTOR_SIZE*2);//父目录的inode修改要同步
	inode_sync(cur_part,parent_dir->inode,io_buf);

	memset(io_buf,0,SECTOR_SIZE*2);//新的dir_entry的inode也要同步
	inode_sync(cur_part,&new_dir_inode,io_buf);
	sys_free(io_buf);
	dir_close(searched_record.parent_dir);
	return 0;
rollback://有条不稳的按顺序回滚 switch是跳转地址表，没有break就按顺序执行
	switch (rollback_step)
	{
	case 3:
		bitmap_set(&cur_part->block_bitmap,block_bitmap_idx,0);
		bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
	case 2:
		bitmap_set(&cur_part->inode_bitmap,inode_no,0);
		bitmap_sync(cur_part,inode_no,INODE_BITMAP);
	case 1:
		dir_close(searched_record.parent_dir);
		sys_free(io_buf);
	}
	return -1;
}
struct dir* sys_opendir(const char*name){
	ASSERT(strlen(name)<=MAX_FILE_NAME_LEN);
	
	struct path_search_record searched_record;
	memset(&searched_record,0,sizeof(searched_record));
	int inode_no=search_file(name,&searched_record);
	struct dir*ret=NULL;
	if(inode_no==0){
		return &root_dir;
	}else if(inode_no==-1){
		printfk("In %s,sub path %s not exist\n",name, ( strrchr(searched_record.searched_path,'/')+1  ));
	}else{
		if(searched_record.file_type==FT_REGULAR){
			printfk("%s is regular file!\n");
		}else if(searched_record.file_type==FT_DIRECTORY){
			ret=dir_open(cur_part,inode_no);
		}
	}
	dir_close(searched_record.parent_dir);
	return ret;
}
int32_t sys_closedir(struct dir*dir){
	int32_t ret=-1;
	if(dir){
		ret=0;
		dir_close(dir);
	}
	return ret;
}
struct dir_entry* sys_readdir(struct dir* dir){
	ASSERT( (dir));
	return dir_read(dir);
}
//此函数将dir里的dir_pos置0，这样readdir返回的就是第一个目录项了
void sys_rewinddir(struct dir*dir){
	ASSERT((dir));
	dir->dir_pos=0;
}
int32_t sys_rmdir(const char*pathname){
	int ret=-1;
	struct path_search_record searched_record;
	memset(&searched_record,0,sizeof(searched_record));
	int inode_no=search_file(pathname,&searched_record);
	if(inode_no==0){
		printfk("can't remove root dir\n");
		dir_close(searched_record.parent_dir);
		return -1;
	}
	if(inode_no==-1){
		printfk("In %s,sub path %s not exist!\n",pathname,  strrchr(searched_record.searched_path,'/')+1 );
		dir_close(searched_record.parent_dir);
		return -1;
	}
	if(searched_record.file_type==FT_REGULAR){
		printfk("%s is regular file!\n",pathname);
		dir_close(searched_record.parent_dir);
		return -1;
	}
	struct dir* child_dir=dir_open(cur_part,inode_no);//打开待删除目录
	if(!dir_is_empty(child_dir)){
		printfk("dir %s is not empty!\n",pathname);
		dir_close(child_dir);
		dir_close(searched_record.parent_dir);
		return -1;
	}
	ret=dir_remove(searched_record.parent_dir,child_dir);
	dir_close(child_dir);
	dir_close(searched_record.parent_dir);
	return ret;
}
static uint32_t get_parent_dir_inode_no(uint32_t child_inode_no,void* io_buf){
	struct inode*child_inode=inode_open(cur_part,child_inode_no);
	uint32_t block_lba=child_inode->i_blocks[0];
	ASSERT(block_lba>=cur_part->sb->data_start_lba);
	inode_close(child_inode);
	ide_read(cur_part->belong_to,block_lba,io_buf,1);
	struct dir_entry* dir_e =(struct dir_entry*)io_buf;
	ASSERT(dir_e[1].i_no<MAX_FILES_PER_PART&&dir_e[1].file_type==FT_DIRECTORY);//0对应. 1对应..
	return dir_e[1].i_no;
}
//以下函数 将p_inode_no对应的父目录中的c_inode_no子目录项找出来，并将其名字  cat（拼接）到 字符串path里
static int32_t get_child_dir_name(uint32_t p_inode_no,uint32_t c_inode_no,char*path,void*io_buf){
	struct inode*p_inode=inode_open(cur_part,p_inode_no);
	uint8_t block_idx=0;
	uint32_t*all_blocks=(uint32_t*)sys_malloc(140*sizeof(uint32_t));
	uint32_t block_cnt=12;
	while(block_idx<12){
		all_blocks[block_idx]=p_inode->i_blocks[block_idx];
		block_idx++;
	}
	if(p_inode->i_blocks[12]){
		block_cnt=140;
		ide_read(cur_part->belong_to,p_inode->i_blocks[12],all_blocks+12,1);
	}
	inode_close(p_inode);

	struct dir_entry*dir_e=(struct dir_entry*)io_buf;
	uint32_t dir_entry_size=cur_part->sb->dir_entry_size;
	uint32_t dir_entry_per_sec=SECTOR_SIZE/dir_entry_size;
	block_idx=0;
	while(block_idx<block_cnt){
		if(all_blocks[block_idx]){
			ide_read(cur_part->belong_to,all_blocks[block_idx],io_buf,1);//这里没有必要memset io_buf，因为每次读出来都覆盖不会数据混乱
			uint8_t dir_entry_idx=0;
			while(dir_entry_idx<dir_entry_per_sec){
				if( (dir_e+dir_entry_idx)->i_no==c_inode_no ){
					strcat(path,"/");
					strcat(path,(dir_e+dir_entry_idx)->file_name);
					sys_free(all_blocks);
					return 0;
				}
				dir_entry_idx++;
			}
		}
		block_idx++;
	}
	sys_free(all_blocks);
	return -1;
}

//以下函数把当前工作路径写入buf， buf大小为size，如果为NULL，则由操作系统malloc 内存保存并返回
char* sys_getcwd(char*buf,uint32_t size){
	ASSERT(buf);
	void* io_buf=sys_malloc(SECTOR_SIZE);//io_buf是给其调用的函数分配的缓冲区
	if(io_buf==NULL){
		printfk("[fs.s]sys_getcwd:malloc memory fail\n");
		return NULL;
	}
	struct task_struct* cur=running_thread();
	int32_t p_inode_no=0;
	int32_t c_inode_no=cur->cwd_inode_no;
	ASSERT(c_inode_no>=0 && c_inode_no<MAX_FILES_PER_PART);
	if(c_inode_no==0){//根目录
		buf[0]='/';
		buf[1]=0;
		return buf;
	}
	memset(buf,0,size);
	char full_path_reverse[MAX_PATH_LEN]={0};//路径缓冲区
	//以下循环获取上一级目录名并写入缓冲区，所以是反着的路径
	while( c_inode_no){
		p_inode_no=get_parent_dir_inode_no(c_inode_no,io_buf);
		if(get_child_dir_name(p_inode_no,c_inode_no,full_path_reverse,io_buf)==0){
			c_inode_no=p_inode_no;
		}else{//如果没找到父目录名
			sys_free(io_buf);
			return NULL;
		}
	}
	ASSERT(strlen(full_path_reverse)<=size);
	char* last_slash;
	while( (last_slash =strrchr(full_path_reverse,'/')  ) ){
		uint32_t len=strlen(buf);
		strcpy(buf+len,last_slash);//每次把 full_path_reverse 中最后的/及以后的部分复制到buf末尾
		*last_slash=0;//把full_path_reverse的结尾/及后面的去掉
	}
	sys_free(io_buf);
	return buf;
}
int32_t sys_chdir(const char* path){
	int32_t ret=-1;
	struct path_search_record searched_record;
	memset(&searched_record,0,sizeof(searched_record));
	int32_t inode_no=search_file(path,&searched_record);
	if(inode_no!=-1){
		if(searched_record.file_type==FT_DIRECTORY){
			running_thread()->cwd_inode_no=inode_no;
			ret=0;
		}else{
			printfk("[fs.c]sys_chdir:%s is not directory\n",path);
			ret=-1;
		}
	}
	dir_close(searched_record.parent_dir);
	return ret;
}
int32_t sys_stat(const char* path,struct stat*buf){
	if(!strcmp(path,"/") || !strcmp(path,"/.")  || !strcmp(path,"/..")  ){
		buf->st_filetype=FT_DIRECTORY;
		buf->st_ino=0;
		buf->st_size=root_dir.inode->i_size;
		return 0;
	}
	int32_t ret=-1;
	struct path_search_record searched_record;
	memset(&searched_record,0,sizeof(searched_record));
	int inode_no=search_file(path,&searched_record);
	if(inode_no!=-1){
		struct inode* tar=inode_open(cur_part,inode_no);
		buf->st_size=tar->i_size;
		buf->st_ino=inode_no;
		buf->st_filetype=searched_record.file_type;
		inode_close(tar);
		ret=0;
	}else{
		printfk("[fs.c]sys_stat:file %s not found\n",path);
	}
	dir_close(searched_record.parent_dir);
	return ret;
}