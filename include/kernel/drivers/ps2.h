#ifndef XOS_PS2_H
#define XOS_PS2_H

#include <stdbool.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

// Controller Commands
#define PS2_CMD_READ_CONFIG 0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_CMD_DISABLE_PORT2 0xA7
#define PS2_CMD_ENABLE_PORT2 0xA8
#define PS2_CMD_DISABLE_PORT1 0xAD
#define PS2_CMD_ENABLE_PORT1 0xAE

// Status Register Bits
#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_STATUS_INPUT_FULL (1 << 1)

// Config Byte Bits
#define PS2_CONFIG_PORT1_INT (1 << 0)
#define PS2_CONFIG_PORT2_INT (1 << 1)
#define PS2_CONFIG_PORT1_CLK (1 << 4)
#define PS2_CONFIG_PORT2_CLK (1 << 5)
#define PS2_CONFIG_TRANSLATION (1 << 6)

#define PS2_IRQ_KEYBOARD 1
#define PS2_IRQ_MOUSE 12

#define PS2_KBD_LSHIFT 0x2A
#define PS2_KBD_RSHIFT 0x36
#define PS2_KBD_RELEASE 0x80

#define PS2_IO_TIMEOUT 10000

bool ps2_init();

#endif