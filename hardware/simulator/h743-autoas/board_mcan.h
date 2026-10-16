#ifndef AUTOAS_BOARD_MCAN_H
#define AUTOAS_BOARD_MCAN_H
#include "CanIf.h"
#include <stdint.h>

/* Diagnostic SRAM reserved by the emulator linker script. */
#define MCU_DEBUG ((volatile uint32_t *)0x24000000u)
#define MCU_STATUS_FD_ACCEPTED 0xCAFEFD11u
#define MCU_STATUS_CLASSIC_ACCEPTED 0xCAFECC09u
#define MCU_STATUS_START_SENDER 0x50000002u
#define MCU_STATUS_START_RECEIVER 0x70000002u
#define MCU_STATUS_ERROR 0xBAD10001u

void mcan_board_init(void);
void mcan_board_poll(void);
void mcan_board_delay(unsigned loops);
#endif
