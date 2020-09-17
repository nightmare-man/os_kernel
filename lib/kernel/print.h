#ifndef _PRINT_H
#define _PRINT_H
#include "stdint.h"
void put_str(char* s);
void put_char(uint8_t char_ascii);
void sleep(uint16_t sec);
void put_int(uint32_t num);
void set_cursor(uint16_t);
uint32_t get_cursor(void);
#endif