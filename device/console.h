#ifndef _DEVICE_CONSOLE_H
#define _DEVICE_CONSOLE_H
#include "../lib/kernel/stdint.h"
void console_init();
void console_accquire();
void console_release();
void console_put_str(char* str);
void console_put_int(uint32_t n);
void console_put_char(char n);
#endif