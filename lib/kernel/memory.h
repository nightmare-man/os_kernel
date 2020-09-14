#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "./stdint.h"
#include "./bitmap.h"
struct virtual_addr{
	struct bitmap vaddr_bitmap;
	uint32_t vaddr_start;
};//定义虚拟内存池 用来管理虚拟内存 分配时bitmap和管理的虚拟地址起始地址

extern struct pool kernel_pool,user_pool;
void mem_init(void);
#endif