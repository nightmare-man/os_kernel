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

extern uint8_t channel_cnt;//在ide.c中定义 在ide_init里初始化
extern struct ide_channel channels[2];//同样
struct partition*cur_part;//记录当前所选用的分区 ，由mout_parttion负责切换
extern struct list partition_list;// 所有分区的链表，定义在ide.c里，由partition_scan初始化


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
	p_de->f_type=FT_DIRECTORY;

	p_de++;//下一个目录项指针位置

	memcpy(p_de,"..",2);
	p_de->i_no=0;
	p_de->f_type=FT_DIRECTORY;

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

static bool mount_partition(struct list_elem*pelem,int arg){
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
	list_traversal(&partition_list,mount_partition,default_part);
}