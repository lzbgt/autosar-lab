/* Educational STM32H743 / Renode FDCAN adapter underneath real AutoAS CanIf.
 * Uses the Bosch M_CAN-compatible register map on Renode v1.17 H743 model.
 * NOT a compliant AUTOSAR Microcontroller Abstraction Layer (MCAL) port:
 * no peripheral clocks, DMA, real bit timing, bus-off recovery, interrupt
 * handling, or TX FIFO scheduling. Single HTH and one in-flight TX only.
 */
#include "board_mcan.h"
#include "Can.h"
#include "CanIf_Can.h"
#include "Det.h"

#define MCAN_BASE 0x4000A000u
#define MSG_RAM_BASE 0x4000AC00u
#define CCCR 0x18u
#define NBTP 0x1Cu
#define DBTP 0x0Cu
#define GFC 0x80u
#define RXF0C 0xA0u
#define RXF0S 0xA4u
#define RXF0A 0xA8u
#define RXESC 0xBCu
#define TXBC 0xC0u
#define TXESC 0xC8u
#define TXBAR 0xD0u
#define TXBTO 0xD8u
#define TX_OFFSET 0x200u
#define FDF (1u << 21)
#define BRS (1u << 20)

static Can_ControllerStateType controller_state = CAN_CS_STOPPED;
static PduIdType pending_pdu;
static uint8_t pending_tx;
static volatile uint32_t *reg32(uint32_t offset) {
    return (volatile uint32_t *)(MCAN_BASE + offset);
}
static volatile uint32_t *ram32(uint32_t offset) {
    return (volatile uint32_t *)(MSG_RAM_BASE + offset);
}
void mcan_board_delay(unsigned loops)
{
    while(loops-- != 0u) { __asm__ volatile("nop" ::: "memory"); }
}
void mcan_board_init(void)
{
    *reg32(CCCR) = 3u;   /* INIT and CCE */
    *reg32(NBTP) = (4u << 16) | (25u << 8) | 5u | (4u << 25);
    *reg32(DBTP) = (1u << 16) | (7u << 8) | (2u << 4) | 2u;
    *reg32(GFC) = 0u;             /* Route unmatched standard messages to RX FIFO0 */
    *reg32(RXF0C) = 2u << 16;     /* Two 64-byte FIFO slots at message RAM offset 0 */
    *reg32(RXESC) = 7u;           /* 64-byte element */
    *reg32(TXBC) = (1u << 16) | TX_OFFSET;
    *reg32(TXESC) = 7u;
    *reg32(CCCR) = (1u << 8) | (1u << 9); /* FD, BRS enabled, INIT cleared */
    pending_tx = 0u;
    MCU_DEBUG[2] = *reg32(CCCR);
}
Std_ReturnType Can_SetControllerMode(uint8_t controller, Can_ControllerStateType mode)
{
    if(controller != 0u || mode != CAN_CS_STARTED) { return E_NOT_OK; }
    mcan_board_init();
    controller_state = CAN_CS_STARTED;
    return E_OK;
}
Std_ReturnType Can_GetControllerMode(uint8_t controller, Can_ControllerStateType *mode)
{
    if(controller != 0u || mode == 0) { return E_NOT_OK; }
    *mode = controller_state;
    return E_OK;
}
Std_ReturnType Can_Write(Can_HwHandleType hth, const Can_PduType *pdu)
{
    if(hth != 0u || pdu == 0 || pdu->sdu == 0 ||
       controller_state != CAN_CS_STARTED) {
        return E_NOT_OK;
    }
    if(pending_tx != 0u) { return CAN_BUSY; }
    const uint8_t length = pdu->length;
    const int fd = (pdu->id & CAN_CANFD_ID_TYPE) != 0u;
    if((length != 8u && length != 16u) || (!fd && length > 8u) ||
       (pdu->id & CAN_CANID_MASK) > 0x7FFu) {
        return E_NOT_OK;
    }
    volatile uint32_t *tx = ram32(TX_OFFSET);
    tx[0] = (pdu->id & 0x7FFu) << 18;
    tx[1] = ((length == 16u ? 10u : 8u) << 16) | (fd ? FDF | BRS : 0u);
    for(unsigned word = 0u; word < length / 4u; ++word) {
        unsigned base = 4u * word;
        tx[word + 2u] = (uint32_t)pdu->sdu[base] |
                        ((uint32_t)pdu->sdu[base + 1u] << 8) |
                        ((uint32_t)pdu->sdu[base + 2u] << 16) |
                        ((uint32_t)pdu->sdu[base + 3u] << 24);
    }
    pending_tx = 1u;
    pending_pdu = pdu->swPduHandle;
    *reg32(TXBAR) = 1u;
    return E_OK;
}

void mcan_board_poll(void)
{
    volatile uint32_t *diag = MCU_DEBUG;
    if(pending_tx && ((*reg32(TXBTO) & 1u) != 0u)) {
        pending_tx = 0u;
        diag[6] = *reg32(TXBTO);
        CanIf_TxConfirmation(pending_pdu);
    }
    uint32_t state = *reg32(RXF0S);
    diag[3] = state;
    if((state & 0x7Fu) == 0u) { return; }
    unsigned index = (state >> 8) & 0x3Fu;
    volatile uint32_t *rx = ram32(index * 72u);
    uint32_t h0 = rx[0], h1 = rx[1];
    unsigned id = (h0 >> 18) & 0x7FFu;
    unsigned dlc = (h1 >> 16) & 0xFu;
    unsigned len = dlc == 10u ? 16u : (dlc <= 8u ? dlc : 0u);
    diag[4] = h0;
    diag[5] = h1;
    if(len != 8u && len != 16u) {
        diag[0] = MCU_STATUS_ERROR;
        *reg32(RXF0A) = index;
        return;
    }
    uint8_t buffer[16];
    for(unsigned i = 0u; i < len; ++i) {
        uint32_t value = rx[2u + i / 4u];
        buffer[i] = (uint8_t)(value >> (8u * (i % 4u)));
    }
    /* MCU-observed ISO-TP Protocol Control Information high nibbles:
     * Single=0, First=1, Consecutive=2, FlowControl=3.
     * Never alter received data or the upper transport state machine. */
    const unsigned pci_type = buffer[0] >> 4;
    if(pci_type < 4u) { MCU_DEBUG[11u + pci_type]++; }
#ifdef AUTOAS_TEST_DROP_FC
    /* Fault injection at the *receiver* driver-to-CanIf boundary. Renode
     * has delivered a real FC to M_CAN FIFO; the driver deliberately does
     * not pass it to the genuine AutoAS CAN Interface. The physical wire
     * layer, acknowledgements and fault timing are not simulated. */
    if(id == 0x731u && pci_type == 3u) {
        MCU_DEBUG[19]++;       /* Raw M_CAN RX observed one dropped FC. */
        *reg32(RXF0A) = index; /* Release the real FIFO element. */
        return;
    }
#endif
    const Can_HwType mailbox = {
        .CanId = id | ((h1 & FDF) ? CAN_CANFD_ID_TYPE : 0u),
        .Hoh = 0u, .ControllerId = 0u
    };
    const PduInfoType pduInfo = {
        .SduDataPtr = buffer, .SduLength = len, .MetaDataPtr = 0
    };
    CanIf_RxIndication(&mailbox, &pduInfo); /* Real AutoAS CanIf dispatch! */
    *reg32(RXF0A) = index;
}

Std_ReturnType Det_ReportError(uint16_t module, uint8_t instance,
                               uint8_t api, uint8_t error)
{
    (void)module; (void)instance; (void)api; (void)error;
    MCU_DEBUG[9] += 1u;
    return E_OK;
}
