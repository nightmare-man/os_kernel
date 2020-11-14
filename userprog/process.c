#include "../lib/kernel/stdint.h"
#include "../lib/kernel/memory.h"
#include "../lib/kernel/global.h"
#include "../thread/thread.h"
#include "../lib/user/process.h"
#include "../lib/kernel/debug.h"
#include "../lib/user/tss.h"
#include "../device/console.h"
#include "../lib/string.h"
#include "../lib/kernel/interrupt.h"
#include "../lib/kernel/print.h"
extern struct list thread_ready_list;//就绪队列
extern struct list thread_all_list;//全部线程队列
extern void intr_exit(void);//定义在kernel.s里中断处理程序的返回部分 用于跳转到新进程
//线程模型：
//timer中断处理程序 call schedule函数 schedule 调用 switch.s 里的switch 然后switch利用将栈顶移到thread_stack最低端pop拿到上下文 ret到kernel_thread函数
//kernel_thread函数调用thread_start里传入的函数（这个调用是thread_stack栈里的上下文准备的）

//而进程模型则是在schedule时检查时新执行流是进程还是线程。如果是进程 就准备进程的页表，并更新esp0（切换进程内核态的上下文）
//然后还是上面那个流程 只不过kerneal_thread 传入的再是start_process 和 filename_ 作为start_process的参数
//这个start_process会设置好intr_stack 也就是中断返回时的栈的信息，然后将esp更新到intr_stack的最底端pop拿到上下文
//通过中断的iret返回 实现特权级的切换（0-3）  （能过切换特权级的就只有 中断返回和任务返回，iret行 ret不行）,到用户代码段执行（特权级3）

//用户进程的中断模型则是，进入中断处理程序时，仍在用户进程内（pdt不变cr3不更新tss的esp0，但是esp会载入tss里esp0的值）
//除非执行schedule

//对于线程的切换 认为ret到kernel_thread时就到了新线程了
//对于用户进程同样是，到了kernel_thread就到了该进程的内核态
//执行完start_process后就到了进程的用户态

//以下函数填充tcb里面的intr_stack上下文 并利用此栈ret到用户进程的用户态
void start_process(void* filename_){
	void*function=filename_;
	struct task_struct*cur=running_thread();
	cur->self_kstack=(uint32_t*)((uint32_t)cur+PG_SIZE);
	cur->self_kstack=(uint32_t*)((uint32_t)cur->self_kstack - sizeof(struct intr_stack));//在创建线程时（进程模型也使用）的create_thread 将self_kstack设置为tcb最顶端
	//减去intr_stack thread_stack大小，让其刚好指向thread_stack最低端，以便后面switch拿到这个地址更新esp 切换执行流
	//现在将这个self_kstack+ struct thread_stack 让其指向intr_stack最低端，
	struct intr_stack*proc_stack=(struct intr_stack*)cur->self_kstack;
	proc_stack->edi=proc_stack->esi=proc_stack->ebp=proc_stack->esp_dummy=1;
	proc_stack->ebx=proc_stack->edx=proc_stack->ecx=proc_stack->eax=1;
	proc_stack->gs=0;
	proc_stack->ds=proc_stack->es=proc_stack->fs=SELECTOR_U_DATA;
	proc_stack->eip=function;
	proc_stack->cs=SELECTOR_U_CODE;
	proc_stack->eflags=(EFLAGS_IOPL_3|EFLAGS_MBS|EFLAGS_IF_1);

	//分配用户态线性地址空间的最高一页，对应的物理页是用户内存池分配的，作为栈区，栈顶指针初始为页的最高地址
	proc_stack->esp=(void*)((uint32_t)get_a_page(PF_USER,USER_STACK3_VADDR)+PG_SIZE);
	proc_stack->ss=SELECTOR_U_DATA;
	asm volatile("movl %0,%%esp;jmp intr_exit"::"g"(proc_stack):"memory");//更新esp 切换到用户进程的内核态的intr_stack
	//从此栈pop拿到上下文 回到新进程的特权级3 代码和特权级3的栈 和数据段
}


//该函数在schedule时被执行，用于切换pdt
//以下函数更新cr3,切换到用户进程的pdt，此函数必先在switch之前调用，因为switch 时已经到了用户进程的内核态了

void page_dir_active(struct task_struct*p_thread){
	//默认是内核线程
	
	uint32_t pagedir_phy_addr=0x100000;
	if(p_thread->pgdir!=NULL){
		//即是用户进程
		pagedir_phy_addr=addr_v2p((uint32_t)p_thread->pgdir);
	}
	asm volatile("movl %0,%%cr3"::"r"(pagedir_phy_addr):"memory");
}

//以下函数为进程在特权级3下运行做准备（改变页表，页表内核态都是一样的，主要是用户态，）
void process_active(struct task_struct* p_thread){
	ASSERT(p_thread!=NULL);
	page_dir_active(p_thread);
	if(p_thread->pgdir){
		update_tss_esp(p_thread);
	}
}
//以下函数从内核分配内存 并构造一个新的pdt，其中pde的高256项（对应内核空间的）都是直接从现有的复制的，最后一项指向新pdt的物理地址
uint32_t* create_page_dir(void){
	//!!!!!
	//进程的pdt也在内核空间 这样用户进程自己无法访问（现在的设置是可以访问的，因为我们的用户代码段描述符的范围是0-4GB
	//并且我们将所有PDE和PTE的US位都设置位1，这样特权级3的进程也可以访问，以后我们需要把0xc0000000以上对应的pde和pte
	//的US位置0，表示内核空间只能由特权级0访问！）
	//!!!!!
	uint32_t* page_dir_vaddr=get_kernel_page(1);
	if(page_dir_vaddr==NULL){
		console_put_str("create_page_dir:get_kernel_page failed!\n");
		return NULL;
	}
	//0xfffff000永远指向pdt自身
	//以下复制0xc0000000以上的内存映射，也就是将pde的768-1023项都复制一遍，最后将1023项指向新pdt的物理地址，重新指向自己
	memcpy( (void*)(  (uint32_t)page_dir_vaddr+0x300*4 ),(const void*)( (uint32_t)0xfffff000+0x300*4 ),0x100*4 );
	uint32_t new_page_dir_phy_addr=addr_v2p( (uint32_t)page_dir_vaddr );//拿到新页表的物理地址
	page_dir_vaddr[1023]=new_page_dir_phy_addr&0xfffff000|PG_US_U|PG_RW_W|PG_P_1;//因为page_dir_vaddr是uint32_t* 因此可以用数组来访问
	return page_dir_vaddr;
}
//以下函数为进程tcb创建一个虚拟内存池的bitmap
void create_user_vaddr_bitmap(struct task_struct* user_prog){
	user_prog->userprog_vaddr.vaddr_start=USER_VADDR_START;//默认分配的程序入口 低于此的虚拟内存空间都不分配
	//以下算出该虚拟内存池位图本身占用多少个page， 向上取整 第一个参数是 所占字节 第二个参数是pgsize
	uint32_t bitmap_pg_cnt=DIV_ROUND_UP(      (0xc0000000-USER_VADDR_START)/PG_SIZE/8,PG_SIZE               );
	user_prog->userprog_vaddr.vaddr_bitmap.bits=get_kernel_page(bitmap_pg_cnt);//给它分配内核空间
	user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len=(0xc0000000-USER_VADDR_START)/PG_SIZE/8;
	bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);//初始化，清零bitmap
}
//以下函数创建用户进程
void process_execute(void* filename,char*name){
	//分配tcb
	struct task_struct* thread=get_kernel_page(1);
	
	//初始化tcb 
	init_thread(thread,name,default_prio);
	thread_create(thread,start_process,filename);//套娃 switch函数拿到新线程stack上下文ret到 thread_create会让执行流到kernel_thread并设置上下文调用start_process并传入参数filename
	//而start_process则通过设置上下文（intr_stack） iret返回到特权级三的代码 filename为ip cs为 start_process里设置的选择子 u_code
	//创建pdt和位图 和初始化block_desc
	create_user_vaddr_bitmap(thread);
	thread->pgdir=create_page_dir();
	block_desc_init(thread->u_block_descs);

	//加入队列
	enum intr_status old_status=intr_disable();
	ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
	list_append(&thread_ready_list,&thread->general_tag);
	ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
	list_append(&thread_all_list,&thread->all_list_tag);
	intr_set_status(old_status);
}
