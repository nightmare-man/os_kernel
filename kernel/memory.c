#include "../lib/kernel/memory.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/global.h"
#include "../lib/string.h"
#include "../lib/kernel/debug.h"
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


#define PDE_IDX(addr) ((addr&0xffc00000)>>22) //获取传入的线性地址的高10位page directory entry
#define PTE_IDX(addr) ((addr&0x003ff000)>>12) //获取传入的线性地址中10位page table entry index



struct pool{
	struct bitmap pool_bitmap;//物理地址 位图
	uint32_t phy_addr_start;
	uint32_t pool_size;//位图能管理的物理位图的大小 虚拟地址pool没有这个 因为虚拟地址必定提供4GB全部的位图
};
struct pool kernel_pool,user_pool;
struct virtual_addr kernel_vaddr;//给内核分配虚拟地址 ，也就是针对内核的PDT(内核被单独作为一个任务 有单独的虚拟地址空间，单独的页表)


//下面这个函数 对虚拟内存池和物理内存池进行初始化 由于暂时还没有实现堆 编译器不支持 变长数组 所以我们对变长位图的实现方式是
//指定一块足够大的内存给它当数组，至于指定哪一块 要根据之前的安排
static void mem_pool_init(uint32_t all_mem){//传入物理内存的实际 大小 loader.s中检测的
	put_str("mem_pool_init start\n");
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

	put_str("kernel_pool_bitmap_start:");
	put_int((uint32_t)kernel_pool.pool_bitmap.bits);
	put_char('\n');

	put_str("kernel_pool_bitmap_len:");
	put_int((uint32_t)kernel_pool.pool_bitmap.btmp_bytes_len);
	put_char('\n');

	put_str("kernel_pool_phy_addr_start:");
	put_int((uint32_t)kernel_pool.phy_addr_start);
	put_char('\n');

	put_str("user_pool_bitmap_start:");
	put_int((uint32_t)user_pool.pool_bitmap.bits);
	put_char('\n');

	put_str("user_pool_bitmap_len:");
	put_int((uint32_t)user_pool.pool_bitmap.btmp_bytes_len);
	put_char('\n');

	put_str("user_pool_phy_addr_start:");
	put_int((uint32_t)user_pool.phy_addr_start);
	put_char('\n');

	bitmap_init(&(kernel_pool.pool_bitmap));
	bitmap_init(&(user_pool.pool_bitmap));

	//给虚拟内存池进行设置
	kernel_vaddr.vaddr_bitmap.btmp_bytes_len=kbm_length;//理论上虚拟内存大小和物理内存大小无关，但是我们
	//这里只考虑--堆--所以给堆的物理大小和虚拟大小一一对应（堆不会被换入换出 虚拟内存和物理内存始终u对应）
	kernel_vaddr.vaddr_bitmap.bits=((void*)MEM_BITMAP_BASE+kbm_length+ubm_length);
	//理论上 物理地址支持512MB 位图占16KB 然后后面1KB是PCB 我们无法紧挨着物理地址的位图来当作虚拟内存的位图
	//但是实际上 我们现在没有传入512MB作为参数 另外 现在还没有使用PCB  ！！！！暂时放在这里！！！
	kernel_vaddr.vaddr_start=K_HEAP_START;//虚拟内存池分配的虚拟起始地址
	bitmap_init(&kernel_vaddr.vaddr_bitmap);
	put_str("mem_pool_init done\n");
}

//以下从内核虚拟内存池申请空间
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt){
	int vaddr_start=0,bit_idx_start=-1;
	uint32_t cnt=0;
	if(pf==PF_KERNEL){
		bit_idx_start=bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt);
		if(bit_idx_start==-1){
			return NULL;
		}
		while(cnt<pg_cnt){
			bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start+cnt,1);//占用找到的空闲位
			cnt++;
		}
		vaddr_start=kernel_vaddr.vaddr_start+bit_idx_start*PG_SIZE;
	}else{
		//用户虚拟内存池暂时未创建 以后再处理
	}
	return ((void*)vaddr_start);//强制类型转换
}

//此函数 返回虚拟地址 对应的 页表项 地址（这个地址仍然是虚拟地址 但是这个虚拟地址 直接指向物理内存中该页表项） 
void* pte_ptr(uint32_t vaddr){
	uint32_t*pte=(uint32_t*)(0xffc00000+((uint32_t)(vaddr&0xffc00000)>>10)+PTE_IDX(vaddr)*4);
	//这个式子原理是 我们要页表项的物理地址 那么反推 该页表项 在页表里，页表也是一个页，
	//我们要该页表的虚拟地址   那么这个页表的虚拟地址 在其页表里的索引是 PDE里的索引
	//所以中10位是原来的高10位 而高10位 由于PDE总是在最后一项指向自己 所以该虚拟地址的
	//页目录索引是0x3ff，放到最高10位就是0xffc00000 最后算上表内偏移， 表内偏移是原页表
	//项的索引 * 4字节/项  原页表索引 用PTE_IDX宏转换得来
	return pte;
}

//此函数用于返回虚拟地址对应的页目录项 的访问地址（虚拟地址）的指针 
void* pde_ptr(uint32_t vaddr){
	uint32_t* pde=(uint32_t*)(0xfffff000+PDE_IDX(vaddr)*4);
	return pde;
}

//在m_pool指向的物理内存池里分配一个物理页
static void* palloc(struct pool*m_pool){
	//书中注明palloc应该是原子的 但是并没有关闭中断。。可能它的意思是 由于
	//每次只操作kernel_phy_bitmap里的一位 因此该操作是原子的吧，毕一次一位
	//这种数据结构的操作，必然是原子的
	int bit_idx=bitmap_scan(&(m_pool->pool_bitmap),1);
	if(bit_idx==-1){
		return NULL;
	}
	bitmap_set(&(m_pool->pool_bitmap),bit_idx,1);
	uint32_t page_phyaddr=((bit_idx*PG_SIZE)+m_pool->phy_addr_start);
	return (void*)page_phyaddr;
}
//以下函数给将 一个物理页 映射到虚拟页 按页映射
static void page_table_add(void*_vaddr,void*_page_phyaddr){
	//0xc010_0000 0x00200000

	uint32_t vaddr=(uint32_t)_vaddr;
	uint32_t page_phyaddr=(uint32_t)_page_phyaddr;
	uint32_t*pde=pde_ptr(vaddr);// fffffc00 fff00400
	uint32_t*pte=pte_ptr(vaddr);//拿到访问这个虚拟地址页目录项和页表项的访问方法
	//需要注意的是 虽然我们已经有了访问 页目录项和页表项的 访问地址 但是并不意味着 它们一定存在，比如
	//页目录项是空的，也就是没有给该项分配页表，这个时候 *pte会报错 因为没有分配
	//但是如果给该页目录项填写了页表的物理地址 也就是分配了页表，那么不论分配的页表物理地址如何，
	//都可以凭 *pte访问对应的项


	//以下是测试用，手动计算并打印结果对比
	// put_str("pde(fffffc00):");
	// put_int((uint32_t)pde);
	// put_char('\n');
	// put_str("pte(fff00400):");
	// put_int((uint32_t)pte);
	// put_char('\n');

	//while(1);
	//另外 我们在loader.s中 给内核虚拟地址的页目录项都分配了页表 是 物理地址0x102000开始的254个页表

	/*
		但是保险起见 还是对*pde先判断 再*pte
	*/

	if((*pde & 0x00000001)){
		ASSERT( !(*pte&0x00000001) );//对应页表项不存在 才需要分配
		if(!(*pte&0x00000001)){
			*pte=(page_phyaddr|PG_US_U|PG_RW_W|PG_P_1);
			
		}else{
			//暂时啥都不做 因为目前的ASSERT能够直接拦截
		}

	}else{
		//此情况PDE不存在
		uint32_t pde_phyaddr=(uint32_t)palloc(&kernel_pool);
		*pde=(pde_phyaddr|PG_US_U|PG_RW_W|PG_P_1);
		memset((void*)((int)pte&0xfffff000),0,PG_SIZE);//这个PTE是一个指针来着 永远指向那个
		//页表项 现在&0xfffff000就指向了那个页表了 所以这个操作将页表清0，实际上这个页表是我们刚
		//从物理内存池分配的 所以要清0
		ASSERT(!(*pte&0x00000001));
		*pte=(page_phyaddr|PG_US_U|PG_RW_W|PG_P_1);

	}
}

//以下函数 分配cnt个页面大小的地址（既分配虚拟地址 又分配物理地址 并且完成映射）
void* malloc_page(enum pool_flags pf,uint32_t cnt){
	ASSERT(cnt>0&&cnt<3840);
	//先分配虚拟地址
	void* vaddr_start=vaddr_get(pf,cnt);
	if(vaddr_start==NULL){
		return NULL;
	}
	uint32_t vaddr=(uint32_t)vaddr_start;
	uint32_t pg_cnt=cnt;

	
	struct pool*mem_pool=pf&PF_KERNEL?&kernel_pool:&user_pool;
	//再分配物理地址 不同的是 物理地址是一页一页分配的 不是连续的
	while(pg_cnt--){
		void*page_phyaddr=palloc(mem_pool);
		put_str("phy addr:");
		put_int((uint32_t)page_phyaddr);
		put_char('\n');
		if(page_phyaddr==NULL){
			return NULL;
		}
		page_table_add((void*)vaddr,page_phyaddr);
		vaddr+=PG_SIZE;
	}
	return vaddr_start;
}

void* get_kernel_page(uint32_t pg_cnt){
	void*vaddr=malloc_page(PF_KERNEL,pg_cnt);
	if(vaddr!=NULL){
		memset(vaddr,0,pg_cnt*PG_SIZE);
	}
	//对分配的内存页进行清零
	return vaddr;
}
void mem_init(){
	put_str("mem_init start\n");
	uint32_t mem_bytes_total =(*(uint32_t*)(MEM_BYTES_TOTAL_ADDR));//之前loader.s里储存在MEM_BYTES_TOTAL处的
	//检测到的物理内存大小
	mem_pool_init(mem_bytes_total);
	put_str("mem_init done\n");
}

