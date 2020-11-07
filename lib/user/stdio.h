#ifndef _USER_STDIO_H_
#define _USER_STDIO_H_
typedef void* va_list;
#include "../../lib/kernel/stdint.h"
void itoa(uint32_t value,char**buf_ptr_addr,uint8_t base);
uint32_t printf(const char*format,...);
#endif