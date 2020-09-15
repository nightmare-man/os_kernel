#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "./stdint.h"
#include "./bitmap.h"
struct virtual_addr{
	struct bitmap vaddr_bitmap;
	uint32_t vaddr_start;
};//定义虚拟内存池 用来管理虚拟内存 分配时bitmap和管理的虚拟地址起始地址

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
#endif