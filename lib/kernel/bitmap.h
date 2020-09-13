#ifndef _LIB_KERNEL_BITMAP_H
#define _LIB_KERNEL_BITMAP_H
#include "./stdint.h"
#define BITMAP_MASK 1
struct bitmap{
	uint32_t btmp_bytes_len;
	uint8_t * bits;//现在没有数组的概念 也没有动态内存 只给个起始地址 自己随便用 ，
	//边界也靠自觉
}
void bitmap_init(struct bitmap* btmp);//bitmap初始化 全0
bool bitmap_scan_test(struct bitmap*btmp,uint32_t bit_idx);//bitmap 的idx位test 看是不是为1
int bitmap_scan(struct bitmap*btmp,uint32_t cnt);//从bitmap找连续的cnt空闲位 返回索引
void bitmap_set(struct bitmap*btmp,uint32_t bit_idx,int8_t value);//将bitmap的idx位设置为value
#endif