#ifndef _LIB_KERNEL_LIST_H
#define _LIB_KERNEL_LIST_H
#define offset(struct_type,member) (int)(&((struct_type*)0)->member);//拿到struct_type这个结构中member成员所在的偏移量
//将0地址强制转换成struct_type的指针 然后对这个结构中的member进行取地址 再转换成int 即是该成员在该结构中的偏移
#define elem2entry(struct_type,struct_member_name,elemp_tr) \
		((struct_type*)((int)elem_ptr-offset(struct_type,struct_member_name)))
		//需要用列表组织的struct_type中，包含一个列表节点 成员  这个宏 就是给定该结构 给定该某一个该类型的变量中的某个成员的地址
		//求出该结构的起始地址 自然是：成员地址-成员偏移

		//为什么这样做呢？而不是把在链表节点中加入数据部分呢？ 原因是 这样做可以最大限度的重用list.h 不用每一中链表都写一份list.h和
		//list.c （c语言原生不支持泛型）
//以下是双向链表
struct list_elem{
	struct list_elem*prev;
	struct list_elem*next;
};
struct list{
	struct list_elem head;
	struct list_elem tail;//head 和 tail不是指针 而是实体 因为作为实体的话声明了编译器就帮我们分配空间了 比较方便
	//指针的话对应的内存可能没有分配 要malloc或者手动再声明一个实体 再&取地址 赋值
};
typedef bool (*function)(struct list_elem*,int arg);
//定义通用函数类型function，以后可以用 function f 来定义一个函数f
//这个函数是回调函数 传入迭代器 list_traversal

void list_init(struct list* ls);
void list_insert_before(struct list_elem*before,struct list_elem*elem);
void list_push(struct list*ls,struct list_elem*elem);
void list_append(struct list*ls,struct list_elem*elem);
void list_remove(struct list_elem*elem);
struct list_elem* list_pop(struct list*ls);
bool elem_find(struct list*ls,struct list_elem*elem);
bool list_empty(struct list*ls);
struct list_elem* list_traversal(struct list*ls,function func,int arg);
uint32_t list_len(struct list*ls);

#endif