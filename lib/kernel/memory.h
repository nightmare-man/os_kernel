#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "./stdint.h"
#include "./bitmap.h"
#include "./list.h"
#define PG_SIZE 4096
struct virtual_addr{
	struct bitmap vaddr_bitmap;
	uint32_t vaddr_start;
};//定义虚拟内存池 用来管理虚拟内存 分配时bitmap和管理的虚拟地址起始地址

struct mem_block{
	struct list_elem free_elem;
	//问：这个结构里为什么只有链表项？这个结构不是空闲内存块吗？大小不应该是对应的块大小吗（16、32、64字节）？

	//答：用声明变量的方式分配内存，是在数据段分配内存的方式，但是我们现在不是在数据段分配，而是从allocate的页中
	//分配，所以我们我们这个mem_block结构只需要表明该块的起始地址就好了，并不需要填满整个块（指定块大小）
	//里面有个链表项，用来将空闲链表串起来，放在内存块描述符的free_list里

};

struct mem_block_desc{
	uint32_t block_size;//内存块大小，规格
	uint32_t block_per_arena;//本arena中最大可容纳的此规格内存块数
	struct list free_list;//空闲mem_block链表
};
//有七种规格的内存块 16 -32 64 128 256 512 1024，因此有7个mem_block_desc
#define DESC_CNT 7


extern struct pool kernel_pool,user_pool;
enum pool_flags{
	PF_KERNEL=1,//枚举类型 用来代表是从哪个内存池分配
	PF_USER=2//额外谈到 为什么 会将物理地址空间分为 两个内存池呢？ 因为内核需要得到保障 不然用户程序一直申请
	//内核可能没有内存可用了
};
#define PG_P_1 1
#define PG_P_0 0
//PTE page table entry 上的p位属性
#define PG_RW_R 0
#define PG_RW_W 2
#define PG_US_S 0 //特权级0才可以访问该页表
#define PG_US_U 4 //任意特权级可访问
void mem_init(void);
void* get_kernel_page(uint32_t pg_cnt);//此函数从内核内存池分配内存
void* get_a_page(enum pool_flags pf,uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);
void block_desc_init(struct mem_block_desc* desc_array);
void* sys_malloc(uint32_t size);//分配块内存空间
void sys_free(void*ptr);
#endif