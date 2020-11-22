#include "../lib/kernel/memory.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/print.h"
#include "../lib/kernel/debug.h"
#include "../lib/kernel/global.h"
#include "../lib/string.h"
#include "../lib/kernel/debug.h"
#include "../thread/sync.h"
#include "../thread/thread.h"
#include "../lib/kernel/interrupt.h"
//页大小
#define MEM_BITMAP_BASE 0xc009a000 
//将物理内存池的位图放置在0x9a000处（物理地址）
//原因是我们 在loader.s里mov esp,0x9f000 认定其是我们内核所占的内存空间的最高地址
//那么我们准备了一个足够表示512MB物理地址的位图 占用 512*1024*1024 / 4096 /8 =16 KB
//即位图自身需要占16KB 4个物理页 
//并且我们还需要在内核空间里为 以后的内核线程实现 留下TCB/PCB 块的空间，也是1个页
//最终 0x9f000 - 5*4096 =0x9a000 
#define K_HEAP_START 0xc0100000 
//这是堆 的起始物理地址 对应的线性地址 我们的堆从1MB以上的物理地址分配（0xc0000000-0xc0001000以下给内核了）


#define PDE_IDX(addr) ((addr&0xffc00000)>>22) //获取传入的线性地址的高10位page directory entry
#define PTE_IDX(addr) ((addr&0x003ff000)>>12) //获取传入的线性地址中10位page table entry index



struct pool{
	struct bitmap pool_bitmap;//物理地址 位图
	uint32_t phy_addr_start;
	uint32_t pool_size;//位图能管理的物理位图的大小 虚拟地址pool没有这个 因为虚拟地址必定提供4GB全部的位图
	struct lock lock;//用于互斥
};

struct arena{
	struct mem_block_desc* desc;//此arena对应的mem_block_desc
	uint32_t cnt;
	bool large;//large为true时，是大内存块（>1024）此时desc为null cnt为arena所占页数（4096一页） 否则 desc正常，cnt为空闲的mem_block数
};
struct mem_block_desc k_block_descs[DESC_CNT];//块描述符表

struct pool kernel_pool,user_pool;//物理内存池
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

//以下从虚拟内存池申请空间 既可以从内核虚拟内存池 也可以从用户虚拟内存池
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
		struct task_struct*cur=running_thread();
		bit_idx_start=bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
		if(bit_idx_start==-1){
			return NULL;
		}
		while(cnt<pg_cnt){
			bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx_start+cnt,1);
			cnt++;
		}
		vaddr_start=cur->userprog_vaddr.vaddr_start+bit_idx_start*PG_SIZE;
		ASSERT( (uint32_t)vaddr_start<=(0xc0000000-PG_SIZE));//分配的块的起始地址上限是0xc0000000-pgsize
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
		if(page_phyaddr==NULL){
			return NULL;
		}
		page_table_add((void*)vaddr,page_phyaddr);
		vaddr+=PG_SIZE;
	}
	return vaddr_start;
}
//以下函数从内核空间分配若干个page
void* get_kernel_page(uint32_t pg_cnt){
	void*vaddr=malloc_page(PF_KERNEL,pg_cnt);
	if(vaddr!=NULL){
		memset(vaddr,0,pg_cnt*PG_SIZE);
	}
	//对分配的内存页进行清零
	return vaddr;
}
//以下函数从用户内存空间分配若干个page（按虚拟地址顺序连续分配的 物理地址则不是）
void* get_user_page(uint32_t pg_cnt){
	lock_acquire(&user_pool.lock);
	void* vaddr=malloc_page(PF_USER,pg_cnt);
	memset(vaddr,0,pg_cnt*PG_SIZE);
	lock_release(&user_pool.lock);
	return vaddr;
}
//以下函数可以指定分配的虚拟地址，但是该函数有个缺点就是如果该虚拟内存块已经被分配了，会被重新分配，获得新的物理内存块 并重新在pdt上对应
void* get_a_page(enum pool_flags pf,uint32_t vaddr){
	struct pool* mem_pool = pf&PF_KERNEL?&kernel_pool:&user_pool;
	lock_acquire(&mem_pool->lock);
	struct task_struct* cur=running_thread();
	int32_t bit_idx=-1;
	if(cur->pgdir!=NULL&&pf==PF_USER){//当前执行流是用户进程，并且申请从用户态内存分配，允许
		bit_idx=(vaddr-cur->userprog_vaddr.vaddr_start)/PG_SIZE;
		ASSERT(bit_idx>0);
		bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx,1);//这里没有检查原来是不是1，因此已经被分配出去的虚拟内存块仍可能再被分配
	}else if(cur->pgdir==NULL&&pf==PF_KERNEL){//当前执行流是内核线程，并申请从内核内存分配，允许
		bit_idx=(vaddr-kernel_vaddr.vaddr_start)/PG_SIZE;
		ASSERT(bit_idx>0);
		bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx,1);
	}else{//对于用户进程申请内核内存和反过来都不允许
		PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace!\n");
	}
	void* page_phyaddr=palloc(mem_pool);
	if(page_phyaddr==NULL){
		return NULL;
	}
	page_table_add((void*)vaddr,(void*)page_phyaddr);
	lock_release(&mem_pool->lock);
	return (void*)vaddr;
}
//以下函数将虚拟地址转换成对应物理地址
uint32_t addr_v2p(uint32_t vaddr){
	uint32_t* pte=pte_ptr(vaddr);//这里直接去拿到这个虚拟地址对应的页表项的虚拟地址，然后访问这个地址拿到表项里的物理地址的高20位（低12位是属性）
	return ((*pte)&0xfffff000)+(vaddr&0x00000fff);
}
//以下函数初始化7个描述符表
void block_desc_init(struct mem_block_desc* desc_array){
	uint16_t desc_idx,block_size=16;
	for(desc_idx=0;desc_idx<DESC_CNT;desc_idx++){
		desc_array[desc_idx].block_size=block_size;
		desc_array[desc_idx].block_per_arena=(PG_SIZE-sizeof(struct arena))/block_size;//页里除了元信息外都用是块
		list_init(&desc_array[desc_idx].free_list);//初始化链表
		block_size*=2;
	}
}

void mem_init(){
	put_str("mem_init start\n");
	uint32_t mem_bytes_total =(*(uint32_t*)(MEM_BYTES_TOTAL_ADDR));//之前loader.s里储存在MEM_BYTES_TOTAL处的
	//检测到的物理内存大小
	mem_pool_init(mem_bytes_total);
	lock_init(&user_pool.lock);
	lock_init(&kernel_pool.lock);
	//对两个物理内存池的锁初始化
	block_desc_init(k_block_descs);
	put_str("mem_init done\n");
}
//以下返回arena中第idx个block的起始地址
static struct mem_block* arena2block(struct arena* a,uint32_t idx){
	return (struct mem_block*)(  (uint32_t)a+sizeof(struct arena)+idx*(a->desc->block_size));
}
//以下返回某个block所属的arena地址
static struct arena* block2arena(struct mem_block*b){
	return (struct arena*)( (uint32_t)b & 0xfffff000);
}

void* sys_malloc(uint32_t size){//以下从内存池中分配字节（即是堆中分配）分配的内存已经清零
	enum pool_flags PF;
	struct pool* mem_pool;
	uint32_t pool_size;
	struct mem_block_desc*descs;
	struct task_struct *cur_thread=running_thread();
	if(cur_thread->pgdir==NULL){//内核线程
		PF=PF_KERNEL;
		pool_size=kernel_pool.pool_size;
		mem_pool=&kernel_pool;
		descs=k_block_descs;
	}else{
		PF=PF_USER;
		pool_size=user_pool.pool_size;
		mem_pool=&user_pool;
		descs=cur_thread->u_block_descs;
	}
	if( !(size>0&&size<pool_size) ){
		return NULL;//超过范围
	}
	struct arena* a;
	struct mem_block*b;
	lock_acquire(&mem_pool->lock);//加锁
	//超过1024就直接分配页
	if(size>1024){
		uint32_t page_cnt=DIV_ROUND_UP(size+sizeof(struct arena),PG_SIZE);
		a=malloc_page(PF,page_cnt);
		if(a!=NULL){
			memset(a,0,page_cnt*PG_SIZE);
			a->desc=NULL;//大内存 元信息里desc为NULL
			a->cnt=page_cnt;
			a->large=true;
			lock_release(&mem_pool->lock);
			return (void*)(a+1);//相当于return (void*)((uint32_t)a+sizeof(struct arena))
		}else{
			//malloc_page出问题
			lock_release(&mem_pool->lock);
			return NULL;
		}
	}else{//正常分配小块内存

		//确定需要的size在哪一个规格
		uint8_t desc_idx;
		for(desc_idx=0;desc_idx<DESC_CNT;desc_idx++){
			if(size<=descs[desc_idx].block_size){
				break;
			}
		}
		//如果该规格的块描述符的空闲链表为空
		//那么分配新的该规格的arena 并分成块加入链表
		if(list_empty(&descs[desc_idx].free_list)){
			a=malloc_page(PF,1);
			//检查分配结果
			if(a==NULL){
				lock_release(&mem_pool->lock);
				return NULL;
			}
			memset(a,0,PG_SIZE);

			a->desc=&descs[desc_idx];//将该arena指向对应规格快描述符
			a->large=false;
			a->cnt=descs[desc_idx].block_per_arena;//不用每次都重新计算一个页的arena能放多少个该规格的block，因为描述符表里有

			uint32_t block_idx;

			//将一个新arena中的所有空闲块加入描述符中的空闲表，为了防止被打断，使用中断
			//为什么不用锁？锁是公共资源，防止互斥时才使用
			//而且一次拿多个锁容易造成死锁

			enum intr_status old_status=intr_disable();
			for(block_idx=0;block_idx<descs[desc_idx].block_per_arena;block_idx++){
				b=arena2block(a,block_idx);
				ASSERT(!elem_find(&descs[desc_idx].free_list,&b->free_elem));//确保之前没被加入（一般不会 毕竟刚创建的arena中分的）
				list_append(&descs[desc_idx].free_list,&b->free_elem);
			}
			intr_set_status(old_status);//关闭中断

		}
		//至此 确定descs[desc_idx].free_list非空
		//开始分配
		
		//从free_list中分配一个list_elem的指针  然后用elem2entry拿到mem_block的起始地址 即使对应块地址
		b=elem2entry(struct mem_block,free_elem,list_pop(&descs[desc_idx].free_list));
		memset(b,0,descs[desc_idx].block_size);//理论上没必要再memset0一次 因为所有的空闲块 都是arena分来的 而arena 被memset0过
		a=block2arena(b);
		a->cnt--;
		lock_release(&mem_pool->lock);
		return (void*)b;
	}
}
//以下函数释放物理内存池占用
void pfree(uint32_t pg_phy_addr){
	struct pool*mem_pool;
	uint32_t bit_idx=0;
	if(pg_phy_addr>=user_pool.phy_addr_start){//物理内存除了已经划归内核(2MB 1mb内核代码数据+1mb页目录页表)以外的部分 都是可分配的堆 分为两个内存池 kernel_pool
	//和user_pool kernel_pool在前 user_pool在后
	//此时代表该物理地址在user_pool
		mem_pool=&user_pool;
		
	}else{
		mem_pool=&kernel_pool;
		
	}
	bit_idx=(pg_phy_addr-mem_pool->phy_addr_start)/PG_SIZE;
	bitmap_set(&mem_pool->pool_bitmap,bit_idx,0);
}
static void page_table_pte_remove(uint32_t vaddr){
	uint32_t* pte=pte_ptr(vaddr);
	*pte &=~PG_P_1;//将页表项p位置0
	asm volatile("invlpg %0"::"m"(vaddr):"memory");//更新tlb（tlb是pdt 的缓存，使用invlpg更新指定页表项的缓存）
}
//以下函数释放vaddr开始的n个虚拟页
static void vaddr_remove(enum pool_flags pf,void*_vaddr,uint32_t pg_cnt){
	uint32_t bit_idx_start=0,vaddr=(uint32_t)_vaddr,cnt=0;
	if(pf==PF_KERNEL){
		bit_idx_start=(vaddr-kernel_vaddr.vaddr_start)/PG_SIZE;
		while(cnt<pg_cnt){
			bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start+cnt++,0);
		}
	}else{
		struct task_struct*cur_thread=running_thread();
		bit_idx_start=(vaddr-cur_thread->userprog_vaddr.vaddr_start)/PG_SIZE;
		while(cnt<pg_cnt){
			bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap,bit_idx_start+cnt++,0);
		}
	}
}

//以下函数 释放虚拟地址vaddr 对应的物理页内存池 映射 和虚拟内存池
void mfree_page(enum pool_flags pf,void* _vaddr,uint32_t pg_cnt){
	uint32_t pg_phy_addr;
	uint32_t vaddr=(uint32_t)_vaddr;
	uint32_t page_cnt=0;//记录释放的物理页数目
	ASSERT(pg_cnt>=1&&vaddr%PG_SIZE==0);//要求必须是页的起始地址
	pg_phy_addr=addr_v2p(vaddr);
	ASSERT((pg_phy_addr%PG_SIZE==0)&&(pg_phy_addr>=0x102000));//0x100000是内核数据代码  而0x100000-0x101000是pdt页目录表 
	//0x101000-0x102000则是低1MB的映射页表，也不能释放
	if(pg_phy_addr>=user_pool.phy_addr_start){
		//位于用户物理内存池
		while(page_cnt<pg_cnt){
			pg_phy_addr=addr_v2p(vaddr+page_cnt*PG_SIZE);
			ASSERT((pg_phy_addr%PG_SIZE==0)&&(pg_phy_addr>=user_pool.phy_addr_start));//确保还在用户物理内存池内（pass：作者这么写的 我觉得
			//没必要再判断）
			pfree(pg_phy_addr);
			page_table_pte_remove(vaddr+page_cnt*PG_SIZE);
			page_cnt++;
		}
		vaddr_remove(PF_USER,_vaddr,pg_cnt);
	}else{
		//位于内核内存池
		while(page_cnt<pg_cnt){
			pg_phy_addr=addr_v2p(vaddr+page_cnt*PG_SIZE);
			ASSERT((pg_phy_addr%PG_SIZE==0)&&(pg_phy_addr>=kernel_pool.phy_addr_start)&&(pg_phy_addr<user_pool.phy_addr_start));
			pfree(pg_phy_addr);
			page_table_pte_remove(vaddr+page_cnt*PG_SIZE);
			page_cnt++;
		}
		vaddr_remove(PF_KERNEL,_vaddr,pg_cnt);
	}
}

//以下函数释放一个块的内存空间，要求传入块的起始地址（！！！如果错误地不是块的起始地址，那么会将以传入的地址为开始地址作为另一个块加入free_list，
//这可能会破坏已经分配出去的块！！）
void sys_free(void*ptr){
	ASSERT(ptr!=NULL);
	enum pool_flags PF;
	struct pool*mem_pool;
	if(running_thread()->pgdir==NULL){
		//如果是内核线程
		ASSERT((uint32_t)ptr>=K_HEAP_START);//内核可以分配的虚拟地址最小值，也就是内核的堆空间起始地址
		PF=PF_KERNEL;
		mem_pool=&kernel_pool;
	}else{
		//对于用户进程的堆的起始地址，只有装载应用程序 exec时才知道，所以这里无法做ASSERT
		PF=PF_USER;
		mem_pool=&user_pool;
	}
	lock_acquire(&mem_pool->lock);//加锁操作物理内存池（！！！没有对pfree加锁，而在调用它的sys_free处加锁！）
	struct mem_block*b=ptr;
	struct arena*a=block2arena(b);//获取元信息 
	ASSERT(a->large==0||a->large==1);//主要是做完整性判断，如果不是这两个值，那么a很可能不是arena元信息
	if(a->desc==NULL&&a->large==true){
		mfree_page(PF,a,a->cnt);
		//如果是大内存块，arena中块描述符为null，不在块描述符 直接 释放这些页
	}else{
		//小于等于1024的内存块
		list_append(&a->desc->free_list,&b->free_elem);//直接放在对应规格的块描述符的freelist中就是释放了
		//然后看看这个块所在的arena是不是全是空块，如果是，顺便把这个arena也释放了
		if(++(a->cnt)==a->desc->block_per_arena){
			uint32_t block_idx;
			for(block_idx=0;block_idx<a->desc->block_per_arena;block_idx++){
				b=arena2block(a,block_idx);
				ASSERT(elem_find(&a->desc->free_list,&b->free_elem));//要arena中的每个块都在空闲链表里，就可以释放这个arena了
				list_remove(&b->free_elem);//free arena后也要在对应的desc->free_list 中remove 块的elem
			}
			mfree_page(PF,a,1);
		}
	}
	lock_release(&mem_pool->lock);//释放物理内存池的锁
}