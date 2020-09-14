#include "../lib/kernel/memory.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/debug.h"

#define PG_SIZE 4096
//页大小
#define MEM_BITMAP_BASE 0xc009a000 
//将物理内存池的位图放置在0x9a000处（物理地址）
//原因是我们 在loader.s里mov esp,0x9f000 认定其是我们内核所占的内存空间的最高地址
//那么我们准备了一个足够表示512MB物理地址的位图 占用 512*1024*1024 / 4096 /8 =16 KB
//即位图自身需要占16KB 4个物理页 
//并且我们还需要在内核空间里为 以后的内核线程实现 留下TCB/PCB 块的空间，也是1个页
//最终 0x9f000 - 5*4096 =0x9a000 
#define K_HEAP_START 0xc0100000 
//这是堆 的起始物理地址 对应的线性地址 我们的堆从1MB以上的物理地址分配（1MB以下给内核了）

struct pool{
	struct bitmap pool_bitmap;//物理地址 位图
	uint32_t phy_addr_start;
	uint32_t pool_size;//位图能管理的物理位图的大小 虚拟地址pool没有这个 因为虚拟地址必定提供4GB全部的位图
}
struct pool kernel_pool,user_pool;
struct virtual_addr kernel_vaddr;//给内核分配虚拟地址 ，也就是针对内核的PDT(内核被单独作为一个任务 有单独的虚拟地址空间，单独的页表)

static void mem_pool_init(uint32_t all_mem){//传入物理内存的实际 大小 loader.s中检测的
	put_str("\nmem_pool_init start\n");
	uint32_t page_table_size = PG_SIZE*256;
	uint32_t used_mem =page_table_size+0x100000;
	//这里的used_mem是怎么来的？
	//已经知道低1MB给内核了和其他设备了 不计入物理内存池  但是仅此而已吗？
	//在loader.s里 我们创建的页表和页目录可是在0x100000上连续分配的，已经分配出去了 256*4096byte大小了
	//分别是 1个页的页目录、1个页的页表（这个页表页表项低256项直接对应低1MB物理页，PTE0和PTE756（也就是内核虚拟地址的第一个页目录项）指向该页表）
	//还有给内核页目录的PTE759-PTE1022提前创建并分配的244个页表，所以一共分配出去了 256个页
	uint32_t free_mem=all_mem - used_mem;
	uint32_t all_free_pages=free_mem/PG_SIZE;
	//对于 不足1页的多余部分并不分配，浪费就浪费算了
	uint32_t kernel_free_pages=all_free_pages/2;
	uint32_t user_free_pages=all_free_pages-kernel_free_pages;

	uint32_t kbm_length=kernel_free_pages/8;//又浪费内存了，bitmap按字节储存长度的 不足一个字节对应的页扔掉了
	uint32_t ubm_length=user_free_pages/8;// 上面是 kernel bitmap length

	uint32_t kp_start=used_mem;//内核 物理地址池的起始地址
	uint32_t up_start=used_mem+kernel_free_pages*PG_SIZE;//用户物理地址池的起始地址

	kernel_pool.phy_addr_start=kp_start;
	user_pool.phy_addr_start=up_start;
	kernel_pool.pool_size=kernel_free_pages*PG_SIZE;
	user_pool.pool_size=user_free_pages*PG_SIZE;
	kernel_pool.pool_bitmap.btmp_bytes_len=kbm_length;
	user_pool.pool_bitmap.btmp_bytes_len=ubm_length;
	kernel_pool.pool_bitmap.bits=(void*)(MEM_BITMAP_BASE);//现阶段没有malloc 直接把已经规划好的位图地址强制转换成void* 拿来当位图
	user_pool.pool_bitmap.bits=(void*)(MEM_BITMAP_BASE+kbm_length);

	put_str("   kernel_pool_bitmap_start:");
	put_int((uint32_t)kernel_pool.pool_bitmap.bits);
	put_str("   kernel_pool_phy_addr_start:");
	put_int((uint32_t)kernel_pool.phy_addr_start);
	put_str('\n');
	put_str("   user_pool_bitmap_start:");
	put_int((uint32_t)user_pool.pool_bitmap.bits);
	put_str("   user_pool_phy_addr_start:");
	put_int((uint32_t)user_pool.phy_addr_start);
	put_char('\n');

	bitmap_init(&(kernel_pool.pool_bitmap));
	bitmap_init(&(user_pool.pool_bitmap));

	//给虚拟内存池进行设置
	kernel_vaddr.vaddr_bitmap.btmp_bytes_len=kbm_length;//理论上虚拟内存大小和物理内存大小无关，但是我们
	//这里只考虑--堆--所以给堆的物理大小和虚拟大小一一对应（堆不会被换入换出 虚拟内存和物理内存始终u对应）
	kernel_vaddr.vaddr_bitmap.bits=((void*)MEM_BITMAP_BASE+kbm_length+ubm_length);
	//理论上 物理地址支持512MB 位图占16KB 然后后面1KB是PCB 我们无法紧挨着物理地址的位图来当作虚拟内存的位图
	//但是实际上 我们现在没有传入512MB作为参数 另外 现在还没有使用PCB 用了就用了，而且我们并没有对虚拟内存池
	//进行初始化

}

