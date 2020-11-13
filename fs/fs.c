#include "./fs.h"
#include "./inode.h"
#include "./dir.h"
#include "./super_block.h"
#include "../device/ide.h"
#include "../lib/kernel/stdio_kernel.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/stdint.h"
#include "../lib/string.h"
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
	sb.maigc=0x19980114;//文件系统类型
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

	 struct disk*hd=part->belong_to;//不用新建一个disk结构，而是直接用指针，实体在ide.c里的有（struct ide_channel数组channels  而channel里有disk数组deives[2]）
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
	for(bit_idx=0;i<block_bitmap_last_bit;i++){
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
	root_dir_inode->i_blocks=sb.data_start_lba;//root_dir 在第0个数据块
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