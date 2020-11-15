#include "./ide.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/timer.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/memory.h"
#include "../lib/string.h"
//以下是ide（integrated dirver electronics 集成电子设备）寄存器 详情可见 第三章
#define reg_data(channel) (channel->port_base+0)
#define reg_error(channel) (channel->port_base+1)
#define reg_sect_cnt(channel) (channel->port_base+2)//该寄存器为8位 如果为0 就会操作256个扇区而不是0个！！！
#define reg_lba_l(channel) (channel->port_base+3)
#define reg_lba_m(channel) (channel->port_base+4)
#define reg_lba_h(channel) (channel->port_base+5)
#define reg_dev(channel) (channel->port_base+6)
#define reg_status(channel) (channel->port_base+7)
#define reg_cmd(channel) (reg_status(channel))
//alternal status
#define reg_alt_status(channel) (channel->port_base+0x206)
//device control
#define reg_ctl_status(channel) (reg_alt_status(channel))

//reg_status 的一些关键位
#define BIT_STAT_BSY 0x80 //硬盘忙
#define BIT_STAT_DRDY 0x40 //驱动器准备好了
#define BIT_STAT_DRQ 0x8 //数据传输准备好了
//device 一些关键位
#define BIT_DEV_MBS 0xa0 //第7和第5位固定为1  
#define BIT_DEV_LBA 0x40 //使用lba 而不是chs
#define BIT_DEV_DEV 0x10

//一些硬盘操作指令
#define CMD_IDENTIFY 0xec
#define CMD_READ_SECTOR 0x20
#define CMD_WRITE_SECTOR 0x30 //读写硬盘

#define max_lba ((80*1024*1024/512)-1) //从0开始计数 因此-1
#define hd_cnt_addr 0x475 //bios记录hd_cnt的地址
uint8_t channel_cnt;//记录通道数 （硬盘数/2） 实际上最多就两个primary channel 和 secondary channel


struct ide_channel channels[2];

uint32_t ext_part_lba_base =0;//扩展分区的起始扇区
uint8_t p_no=0,l_no=0;//记录硬盘主分区和逻辑分区下标
struct list partition_list;//分区链表

struct partition_table_entry{//dpt项结构
	uint8_t bootable;
	uint8_t start_head;
	uint8_t start_sec;
	uint8_t start_chs;//起始柱面号
	uint8_t fs_type;
	uint8_t end_head;
	uint8_t end_sec;
	uint8_t end_chs;
	//下面两项重要
	uint32_t start_lba;
	uint32_t sec_cnt;
} __attribute__((packed));//关闭内存对齐，按照16字节编译

struct boot_sector{//mbr扇区结构
	uint8_t text[446];//代码
	struct partition_table_entry dpt[4];//dpt
	uint16_t signature; //0x55 0xaa 结束标志
} __attribute__((packed));




//以下函数切换工作硬盘 主/从
static void select_disk(struct disk*hd){
	uint8_t reg_device=BIT_DEV_MBS|BIT_DEV_LBA;//构造device寄存器高4位 默认 device 位为0 主盘
	if(hd->dev_no==1){
		reg_device|=BIT_DEV_DEV;//置1 从盘
	}
	outb(reg_dev(hd->my_channel),reg_device);
}

static void select_sector(struct disk*hd,uint32_t lba,uint8_t sec_cnt){
	ASSERT(lba<=max_lba);
	struct ide_channel*channel=hd->my_channel;
	outb(reg_sect_cnt(channel),sec_cnt);//写入要操作的扇区数

	outb(reg_lba_l(channel),lba & 0xff);//写入lba地址0-23位
	outb(reg_lba_m(channel),(lba>>8) & 0xff);
	outb(reg_lba_h(channel),(lba>>16) & 0xff);

	//写入lba地址24-27位
	outb(reg_dev(channel),BIT_DEV_MBS|BIT_DEV_LBA| (hd->dev_no==1?BIT_DEV_DEV:0)|lba>>24);//device寄存器低4位存着lba 24-27位
}

//以下函数向channel发送cmd
static void cmd_out(struct ide_channel* channel,uint8_t cmd){
	channel->expecting_intr=true;//发送命令后 置该位true，以便于中断时知道是哪个硬盘完成了命令
	outb(reg_cmd(channel),cmd);
}


//以下函数读入cnt个sector到buf 注意 此函数 不设置lba地址寄存器 不设置 sector_cnt寄存器
//sec_cnt为0时会操作256个而不是0个
//不作为正式读磁盘接口
static void read_from_sector(struct disk*hd,void* buf,uint8_t sec_cnt){
	uint32_t size_in_byte;
	if(sec_cnt==0){
		size_in_byte=256*512;
	}else{
		size_in_byte=sec_cnt*512;
	}
	insw(reg_data(hd->my_channel),buf,size_in_byte/2);
}

//以下函数写入cnt个sector到buf 同上
static void write_to_sector(struct disk*hd,void* buf,uint8_t sec_cnt){
	uint32_t size_in_byte;
	if(sec_cnt==0){
		size_in_byte=256*512;
	}else{
		size_in_byte=sec_cnt*512;
	}
	outsw(reg_data(hd->my_channel),buf,size_in_byte/2);
}

//以下函数 最多等待（休眠）30s，然后返回cmd执行状态
static bool busy_wait(struct disk*hd){
	struct ide_channel* channel=hd->my_channel;
	uint32_t time_limit=30*1000;
	while((time_limit-=10)>=0){
		if(!(inb(reg_status(channel))&BIT_STAT_BSY)){//如果status bsy位置1 命令仍在执行
			return (inb(reg_status(channel))&BIT_STAT_DRQ);//返回DRQ状态
		}else{
			milliseconds_sleep(10);
		}
	}
	return false;//如果30s过去没drq置1 那就timeout
}
void ide_read(struct disk*hd,uint32_t lba,void*buf,uint32_t sec_cnt){
	ASSERT(lba<=max_lba);
	ASSERT(sec_cnt>0);
	lock_acquire(&hd->my_channel->lock);//因为有两个硬盘，但是共用一个通道 公共资源 防止其他线程
	//1  选disk
	select_disk(hd);
	
	uint32_t secs_op;//每次操作的sector 数 （sec_cnt有可能远大于256）
	uint32_t secs_done=0;//完成数
	while(secs_done<sec_cnt){
		if((secs_done+256)<=sec_cnt){
			secs_op=256;
		}else{
			secs_op=sec_cnt-secs_done;
		}
		//2 选lba sec_cnt
		select_sector(hd,lba+secs_done,secs_op);

		//3 read
		cmd_out(hd->my_channel,CMD_READ_SECTOR);

		//4 立即阻塞自己 等待硬盘完成读取后发起中断唤醒
		sema_down(&hd->my_channel->disk_done);//用sema_down而不是thread_block 是为了方便唤醒， struct semaphore中维护了block在此的thread
		//链表，容易unblock，单纯block 不好unblock
		if( !busy_wait(hd)){//如果发现读取不了
			char error[64];
			sprintf(error,"%s read sector %d failed!\n",hd->name,lba);
			PANIC(error);
		}
		//5 从缓冲区中读出
		read_from_sector(hd,(void*)(  (uint32_t)buf + secs_done*512  ),secs_op);

		secs_done+=secs_op;
	}
	lock_release(&hd->my_channel->lock);//释放通道
}


void ide_write(struct disk*hd,uint32_t lba,void*buf,uint32_t sec_cnt){
	ASSERT(lba<=max_lba);
	ASSERT(sec_cnt>0);
	lock_acquire(&hd->my_channel->lock);//因为有两个硬盘，但是共用一个通道 公共资源 防止其他线程
	//1  选disk
	select_disk(hd);
	
	uint32_t secs_op;//每次操作的sector 数 （sec_cnt有可能远大于256）
	uint32_t secs_done=0;//完成数
	while(secs_done<sec_cnt){
		if((secs_done+256)<=sec_cnt){
			secs_op=256;
		}else{
			secs_op=sec_cnt-secs_done;
		}
		//2 选lba sec_cnt
		select_sector(hd,lba+secs_done,secs_op);

		//3 read
		cmd_out(hd->my_channel,CMD_WRITE_SECTOR);


		//写硬盘则不用准备 所以不用阻塞自己 这一步
		// //4 立即阻塞自己 等待硬盘完成读取后发起中断唤醒
		// sema_down(&hd->my_channel->disk_done);//用sema_down而不是thread_block 是为了方便唤醒， struct semaphore中维护了block在此的thread
		//链表，容易unblock，单纯block 不好unblock
		if( !busy_wait(hd)){//如果发现读取不了
			char error[64];
			sprintf(error,"%s read sector %d failed!\n",hd->name,lba);
			PANIC(error);
		}
		//4 从缓冲区中读出
		write_to_sector(hd,(void*)(  (uint32_t)buf + secs_done*512  ),secs_op);

		//5 写完数据后阻塞自己 等中断唤醒
		sema_down(&hd->my_channel->disk_done);

		secs_done+=secs_op;
	}

	lock_release(&hd->my_channel->lock);//释放通道
}
void intr_hd_handler(uint8_t irq_no){
	
	ASSERT(irq_no==(0x2e)||irq_no==(0x2f));// irq14 15中断码
	uint8_t ch_no=irq_no-0x2e;
	struct ide_channel* channel=&channels[ch_no];
	ASSERT(channel->irq_no==irq_no);
	if(channel->expecting_intr){
		channel->expecting_intr=false;
		sema_up(&channel->disk_done) ;
		
		//读status 让硬盘知道此中断已经被处理 继续操作
		inb(reg_status(channel));
	}
	
}

//此函数交换一个字符串里相邻两个字节的位置
static void swap_pairs_bytes(const char*dst,char*buf,uint32_t len){
	uint8_t idx;
	for(idx=0;idx<len;idx+=2){
		buf[idx+1]=*dst++;//idx+1可能会越界，要求 len为偶数才行
		buf[idx]=*dst++;
	}
	buf[idx]='\0';
}

//to-fix
//2020-11-11：invalid opcode exception 
//推测为 缓冲区溢出导致此函数ret错误位置！（因为调用此函数前可以printfk，此函数最后一句也能正常执行，但是返回就错误）
//另外printfk过buf变量 ，我觉得应该给sprintf加一个防止缓冲区溢出检测！！！

static void identify_disk(struct disk*hd){
	char id_info[512];//保存identify返回的512字节数据
	select_disk(hd);
	
	cmd_out(hd->my_channel,CMD_IDENTIFY);
	//相当于读数据，因此发送命令后立马阻塞自己 不用yeild是因为yeild后大概率还是没完成，因此等中断unblock比较合适
	sema_down(&hd->my_channel->disk_done);
	
	if(!busy_wait(hd)){
		char error[512];
		
		sprintf(error,"%s identify failed!\n",hd->name);
		
		PANIC(error);
	}
	read_from_sector(hd,id_info,1);
	char buf[64];
	uint8_t sn_start=10*2,sn_len=20,md_start=27*2,md_len=40;
	swap_pairs_bytes(&id_info[sn_start],buf,sn_len);
	printfk("   SN: %s\n",buf);
	memset(buf,0,sizeof(buf));
	swap_pairs_bytes(&id_info[md_start],buf,md_len);
	printfk("  MODULE: %s\n",buf);
	uint32_t sectors=*(uint32_t*)&id_info[60*2];
	printfk("  SECTORS: %d\n",sectors);
	printfk("  CAPACITY: %dMB\n",sectors*512/1024/1024);
	
}

//以下函数递归的扫描分区信息， 传入hd ext_lba(mbr/ebr所在扇区lba)
static void partition_scan(struct disk* hd,uint32_t ext_lba){
	struct boot_sector *bs=sys_malloc(sizeof(struct boot_sector));//我们的栈（内核线程的 都在pcb里 和其他结构一起不能超过4096不然溢出）
	//况且这个partition_scan还是递归调用的 因此得malloc
	ide_read(hd,ext_lba,bs,1);//读入mbr/ebr
	uint8_t part_idx=0;
	struct partition_table_entry*p=bs->dpt;
	while(part_idx<4){
		if(p[part_idx].fs_type==0x05){
			//是扩展分区
			if(ext_part_lba_base==0){
				//即第一次scan 这个扩展分区是主扩展分区
				ext_part_lba_base=p[part_idx].start_lba;//更新扩展分区偏移地址
				partition_scan(hd,p[part_idx].start_lba);//递归
			}else{
				partition_scan(hd,p[part_idx].start_lba+ext_part_lba_base);//那么这是扩展分区链，指向下一个扩展分区
			}

		}else if(p[part_idx].fs_type!=0){
			//有效分区类型
			if(ext_lba==0){
				ASSERT(p_no<4);
				//此时是mbr记录的主分区
				hd->prim_parts[p_no].start_lba=ext_lba+p[part_idx].start_lba;//真的离谱，ext_lba都为0了 还搁这儿+
				hd->prim_parts[p_no].sec_cnt=p[part_idx].sec_cnt;
				hd->prim_parts[p_no].belong_to=hd;
				list_append(&partition_list,&hd->prim_parts[p_no].part_tag);//加入分区链表;
				sprintf(hd->prim_parts[p_no].name,"%s%d",hd->name,p_no+1);
				printfk("name:%s\n",hd->prim_parts[l_no].name);
				p_no++;
				
			}else{
				if(l_no>7){
					return;//只支持8个逻辑分区
				}
				//逻辑分区
				hd->logic_parts[l_no].start_lba=ext_lba+p[part_idx].start_lba;//真的离谱，ext_lba都为0了 还搁这儿+
				hd->logic_parts[l_no].sec_cnt=p[part_idx].sec_cnt;
				hd->logic_parts[l_no].belong_to=hd;
				list_append(&partition_list,&hd->logic_parts[l_no].part_tag);
				
				sprintf(hd->logic_parts[l_no].name,"%s%d",hd->name,l_no+5);// 逻辑分区标号从5开始
				printfk("name:%s\n",hd->logic_parts[l_no].name);
				l_no++;
			}
		}
		part_idx++;
	}
	sys_free(bs);//释放保存mbr ebr的内存，分区都在hd->logic_parts 和hd->prim_parts链里
}
static bool partition_info(struct list_elem*pelem,int32_t arg_UNUSED){
	struct partition*part=elem2entry(struct partition,part_tag,pelem);
	printfk("   %s start_lba:0x%x, sec_cnt:0x%x\n",part->name,part->start_lba,part->sec_cnt);
	return false;
}


//初始化ide
void ide_init(){
	printfk("ide_init start\n");
	uint8_t hd_cnt= *((uint8_t*)hd_cnt_addr);
	ASSERT(hd_cnt>0);
	channel_cnt=DIV_ROUND_UP(hd_cnt,2);
	
	struct ide_channel* channel;
	uint8_t channel_no=0;
	uint8_t dev_no=0;

	//初始化通道channels
	while(channel_no<channel_cnt){
		channel=&channels[channel_no];
		sprintf(channel->name,"ide%d",channel_no);
		switch(channel_no){
			case 0:
				channel->port_base=0x1f0;
				channel->irq_no=0x20+14;// irq0对应中断码是0x20 irq14 0x20+14  
				break;
			case 1:
				channel->port_base=0x170;
				channel->irq_no=0x20+15;
				break;
		}
		channel->expecting_intr=false;
		lock_init(&channel->lock);
		sema_init(&channel->disk_done,0);

		//注册irq14 15 handler
		register_handler(channel->irq_no,intr_hd_handler);
		list_init(&partition_list);
		//初始化磁盘channel->devces

		while(dev_no<2){
			struct disk*hd=&channel->devices[dev_no];
			hd->my_channel=channel;
			hd->dev_no=dev_no;
			
			sprintf(hd->name,"sd%c",'a'+channel_no*2+dev_no);//设置默认磁盘名
			identify_disk(hd);
			if(dev_no==1){//只扫描第二个磁盘，因为我们第一个磁盘没分区
				partition_scan(hd,0);//
			}
			p_no=0;
			l_no=0;
			
			dev_no++;
		}
		
		dev_no=0; //这些置0是为了再次对某个硬盘使用partition_scan，实际上我们只对一个硬盘使用，因此用不上
		channel_no++;
	}
	printfk("\n   all partition info\n");
	list_traversal(&partition_list,partition_info,(int32_t)NULL);
	printfk("ide_init done\n");
}

