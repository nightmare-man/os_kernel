#ifndef _USER_TSS_H
#define _USER_TSS_H
//关于自定义struct 什么时候需要放到.h 文件什么时候需要放到.c文件
//如果该结构会在别的调用.h的.c文件里定义变量 那就在.h文件里 否则还是定义
//.c文件里模块自己用就行了
void tss_init();
#endif