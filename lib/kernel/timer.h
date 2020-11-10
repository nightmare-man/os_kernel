#ifndef _TIMER_H
#define _TIMER_H
#include "./stdint.h"


void frequency_set(uint8_t counter_port,uint8_t counter_no,uint8_t rwl,uint8_t counter_mode,uint16_t counter_value);
void timer_init();
void milliseconds_sleep(uint32_t mil_secs);
#endif