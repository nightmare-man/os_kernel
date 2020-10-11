#ifndef _LIB_STDINT_H
#define _LIB_STDINT_H
#define NULL ((void*)(0))
#define true 1
#define false 0
//DIV_ROUND_UP即向上取整
#define DIV_ROUND_UP(X,STEP) ((X+STEP-1)/STEP)
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t; //32位下 long 是32位的 long long 才是64位的
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef int8_t bool;

#endif