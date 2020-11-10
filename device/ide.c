#include "./ide.h"
#include "../lib/kernel/io.h"
#include "../lib/kernel/timer.h"
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

#define max_lba ((80*1024*1024/512)-1)
#define hd_cnt_addr 0x475 //bios记录hd_cnt的地址
uint8_t channel_cnt;//记录通道数 （硬盘数/2） 实际上最多就两个primary channel 和 secondary channel


struct ide_channel channels[2];
void ide_init(){
	printfk("ide_init start\n");
	uint8_t hd_cnt= *((uint8_t*)hd_cnt_addr);
	ASSERT(hd_cnt>0);
	channel_cnt=DIV_ROUND_UP(hd_cnt,2);
	struct ide_channel* channel;
	uint8_t channel_no=0;
	while(channel_no<channel_cnt){
		channel=&channels[channel_no];
		sprintf(channel->name,"ide%d",channel_no);
		switch(channel_no){
			case 0:
				channel->port_base=0x1f0;
				channel->irq_no=0x20+14;//在从片的irq14
				break;
			case 1:
				channel->port_base=0x170;
				channel->irq_no=0x20+15;
				break;
		}
		channel->expecting_intr=false;
		lock_init(&channel->lock);
		sema_init(&channel->disk_done,0);
		channel_no++;
	}
	printfk("ide_init done\n");
}


//以下函数切换工作硬盘 主/从
static void select_disk(struct disk*hd){
	uint8_t reg_device=BIT_DEV_MBS|BIT_DEV_LBA;//构造device寄存器高4位 默认 device 位为0 主盘
	if(hd->dev_no==1){
		reg_device|=BIT_DEV_DEV;//置1 从盘
	}
}

static void select_sector(struct disk*hd,uint32_t lba,uint8_t sec_cnt){
	ASSERT(lba<max_lba);
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
	insw(reg_data(hd->my_channel),buf,size_in_byte);
}

//以下函数写入cnt个sector到buf 同上
static void write_to_sector(struct disk*hd,void* buf,uint8_t sec_cnt){
	uint32_t size_in_byte;
	if(sec_cnt==0){
		size_in_byte=256*512;
	}else{
		size_in_byte=sec_cnt*512;
	}
	outsw(reg_data(hd->my_channel),buf,size_in_byte);
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
