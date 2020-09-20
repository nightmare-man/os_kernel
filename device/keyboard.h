#ifndef _DEVICE_KEYBOARD_H
#define _DEVICE_KEYBOARD_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/interrupt.h"
#define KBY_BUF_PORT 0x60
void keyboard_init(void);
#endif