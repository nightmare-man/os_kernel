#include "./ide.h"

#define reg_data(channel) (channel->port_base+0)
#define reg_error(channel) (channel->port_base+1)
#define reg_sect_cnt(channel) (channel->port_base+2)
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

//reg_alt_status 的一些关键位
#define BIT_ALT_STAT_BSY 0x80 //硬盘忙
#define BIT_ALT_STAT_DRDY 0x40 //驱动器准备好了
#define BIT_ALT_STAT_DRQ 0x8 //数据传输准备好了
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