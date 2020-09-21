#ifndef _DEVICE_KEYBOARD_H
#define _DEVICE_KEYBOARD_H
#include "../lib/kernel/stdint.h"
#include "../lib/kernel/interrupt.h"
#define KBD_BUF_PORT 0x60
extern struct ioqueue kbd_buf;

void keyboard_init(void);
#endif