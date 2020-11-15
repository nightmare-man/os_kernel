#include "../lib/kernel/list.h"
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/interrupt.h"
void list_init(struct list* ls){
	ls->head.prev=NULL;
	ls->head.next=&ls->tail;
	ls->tail.prev=&ls->head;
	ls->tail.next=NULL;
}

//以下函数把链表元素elem 插入到before【之前】
void list_insert_before(struct list_elem*before,struct list_elem*elem){
	enum intr_status old_status=intr_disable();
	//关闭中断 并保存原来的中断状态;

	//对于一些一个进程内，多个线程都能访问的公共资源 称为
	//临界区 对临界区的访问为了保证安全 应该是原子的，即要么不执行 要么全部执行完
	//实现原子操作 要么关中断 要么加互斥锁  （另一个与之类似的是可重入函数 可重入函数是 要么不涉及临界区 要么涉及临界区但是保证原子）
	before->prev->next=elem;
	elem->prev=before->prev;
	elem->next=before;
	before->prev=elem;

	intr_set_status(old_status);
}
//添加元素到链表的最前面（head的后面）
void list_push(struct list*ls,struct list_elem*elem){
	list_insert_before(ls->head.next,elem);
}
//添加元素到链表的最后面（tail的前面）
void list_append(struct list*ls,struct list_elem*elem){
	list_insert_before(&ls->tail,elem);
}

//从链表中移除元素elem
void list_remove(struct list_elem*elem){
	enum intr_status old_status=intr_disable();
	//同样是首先关闭中断
	elem->prev->next=elem->next;
	elem->next->prev=elem->prev;

	intr_set_status(old_status);
}
//将链表的第一个元素（head后面那个）取出
struct list_elem* list_pop(struct list*ls){
	struct list_elem* elem=ls->head.next;
	list_remove(elem);
	return elem;
}
//以下从ls链表查找一个节点elem
bool elem_find(struct list*ls,struct list_elem*elem){
	struct list_elem*temp=ls->head.next;//指向第一个
	while(temp!=&(ls->tail)){
		if(temp==elem){
			return true;
		}
		temp=temp->next;//2020/9/18 bug fixed:must points to next
	}
	return false;
}
bool list_empty(struct list*ls){
	return (ls->head.next==&(ls->tail));
}

//以下函数为迭代器：遍历链表，对每个元素调用 传入参数是 elem和arg 如果该元素满足条件就返回true 
struct list_elem* list_traversal(struct list*ls,function func,int32_t arg){
	struct list_elem* elem=ls->head.next;//默认是第一个
	if(list_empty(ls)){
		return NULL;
	}
	while(elem!=&(ls->tail)){
		if(func(elem,arg)){
			return elem;
		}
		elem=elem->next;
	}
	return NULL;
}
uint32_t list_len(struct list*ls){
	struct list_elem* elem=ls->head.next;
	uint32_t cnt=0;
	while(elem!=&(ls->tail)){
		cnt++;
		elem=elem->next;
	}
	return cnt;
}

