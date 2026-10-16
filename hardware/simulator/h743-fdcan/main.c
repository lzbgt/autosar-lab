/* Cortex-M7 firmware for Renode's STM32H743 CAN.MCAN model.
 * Freestanding M_CAN register exercise. NOT a generic STM32 HAL/MCAL driver.
 * The Renode internal CANHub delivers frames; physical CAN bit timing
 * and transceiver faults are not simulated.
 */
#include <stdint.h>
#ifndef SIM_ROLE
#error Define SIM_ROLE: 0 sender, 1 receiver
#endif

#define MCAN_BASE 0x4000A000u
#define MSG_RAM_BASE 0x4000AC00u
#define STATUS_ADDR 0x24000000u
#define CAN_CCCR 0x18u
#define CAN_NBTP 0x1Cu
#define CAN_DBTP 0x0Cu
#define CAN_GFC 0x80u
#define CAN_RXF0C 0xA0u
#define CAN_RXF0S 0xA4u
#define CAN_RXF0A 0xA8u
#define CAN_RXESC 0xBCu
#define CAN_TXBC 0xC0u
#define CAN_TXESC 0xC8u
#define CAN_TXBAR 0xD0u
#define CAN_TXBTO 0xD8u
#define TX_RAM_OFFSET 0x200u
#define FDF (1u << 21)
#define BRS (1u << 20)

static volatile uint32_t * const debug = (volatile uint32_t *)STATUS_ADDR;
static inline volatile uint32_t *reg32(uint32_t off) {
    return (volatile uint32_t *)(MCAN_BASE + off);
}
static inline volatile uint32_t *ram32(uint32_t off) {
    return (volatile uint32_t *)(MSG_RAM_BASE + off);
}
static void spin(unsigned n) {
    while(n-- != 0u) {
        __asm__ volatile("nop" ::: "memory");
    }
}
static void mcan_init(void) {
    *reg32(CAN_CCCR) = 3u; /* INIT, CCE configuration bits */
    *reg32(CAN_NBTP) = (4u << 16) | (25u << 8) | (5u << 0) | (4u << 25);
    *reg32(CAN_DBTP) = (1u << 16) | (7u << 8) | (2u << 4) | (2u << 0);
    *reg32(CAN_GFC) = 0u;          /* Non-matching standard -> FIFO0 */
    *reg32(CAN_RXF0C) = (2u << 16);/* Two 64-byte elements, offset 0 */
    *reg32(CAN_RXESC) = 7u;        /* 64 bytes in RX FIFO0 */
    *reg32(CAN_TXBC) = (1u << 16) | TX_RAM_OFFSET; /* 1 TX element */
    *reg32(CAN_TXESC) = 7u;        /* 64 bytes TX */
    *reg32(CAN_CCCR) = (1u << 8) | (1u << 9);  /* FD & BRS; INIT cleared */
    debug[2] = *reg32(CAN_CCCR);
}
static void transmit(uint32_t identifier, unsigned bytes) {
    volatile uint32_t *tx = ram32(TX_RAM_OFFSET);
    const unsigned fd = bytes > 8u;
    const unsigned dlc = fd ? 10u : 8u;  /* DLC 10 => 16 bytes */
    tx[0] = identifier << 18; /* 11-bit standard ID encoded in T0[28:18] */
    tx[1] = (dlc << 16) | (fd ? FDF | BRS : 0u);
    if(fd) {
        tx[2] = 0x13121110u;
        tx[3] = 0x17161514u;
        tx[4] = 0x1B1A1918u;
        tx[5] = 0x1F1E1D1Cu;
    } else {
        tx[2] = 0x44332211u;
        tx[3] = 0x88776655u;
    }
    *reg32(CAN_TXBAR) = 1u;
}

static int poll_receive(void) {
    const uint32_t state = *reg32(CAN_RXF0S);
    debug[3] = state;
    if((state & 0x7Fu) == 0u) {
        return 0;
    }
    const unsigned index = (state >> 8) & 0x3Fu;
    volatile uint32_t *rx = ram32(index * 72u);
    const uint32_t header0 = rx[0], header1 = rx[1];
    const unsigned id = (header0 >> 18) & 0x7FFu;
    const unsigned dlc = (header1 >> 16) & 0xFu;
    const int is_fd = (header1 & FDF) != 0u;
#if SIM_ROLE == 1
    const int brs = (header1 & BRS) != 0u;
#endif
    debug[4] = header0;
    debug[5] = header1;
    int passed = 0;
#if SIM_ROLE == 1
    passed = id == 0x201u && dlc == 10u && is_fd && brs &&
             rx[2] == 0x13121110u && rx[5] == 0x1F1E1D1Cu;
    debug[0] = passed ? 0xCAFEFD10u : 0xBAD00001u;
#else
    passed = id == 0x202u && dlc == 8u && !is_fd &&
             rx[2] == 0x44332211u && rx[3] == 0x88776655u;
    debug[0] = passed ? 0xCAFECC08u : 0xBAD00002u;
#endif
    *reg32(CAN_RXF0A) = index;  /* Acknowledge this FIFO slot */
    return passed;
}
int main(void) {
    debug[0] = SIM_ROLE ? 0x70000001u : 0x50000001u;
    debug[1] = *reg32(0u);  /* M_CAN release register */
    debug[2] = debug[3] = debug[4] = debug[5] = debug[6] = 0u;
    mcan_init();
    debug[0] = SIM_ROLE ? 0x70000002u : 0x50000002u;
#if SIM_ROLE == 0
    unsigned attempts = 0u;
#endif
    for(;;) {
        if(poll_receive()) {
#if SIM_ROLE == 1
            transmit(0x202u, 8u); /* Reply with Classical CAN */
#endif
            for(;;) {
                debug[6] = *reg32(CAN_TXBTO);
                __asm__ volatile("nop");
            }
        }
#if SIM_ROLE == 0
        if((attempts++ % 200u) == 0u) {
            transmit(0x201u, 16u); /* Send genuine CAN FD DLC 10 + BRS */
            debug[6] = *reg32(CAN_TXBTO);
        }
#endif
        spin(5000u);
    }
}
