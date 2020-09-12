#ifndef _STRING_H
#define _STRING_H
#include "./kernel/stdint.h"
void memset(void*dst_,uint8_t value,uint32_t size);
void memcpy(void*dst_,const void* src_,uint32_t size);
int8_t memcmp(const void*a_,const void * b_,uint32_t size);
char* strcpy(char*dst_,const char*src_);
uint32_t strlen(const char* str);
int8_t strcmp(const char*a,const char* b);
char* strchr(const char* str,const char ch);
char* strrchr(const char*str,const char ch);
char * strcat(char*dst_,char*src_);
uint32_t strchrs(const char*str,const char ch);
#endif