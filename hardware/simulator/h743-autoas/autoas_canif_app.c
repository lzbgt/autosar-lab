/* Real AutoAS CanIf.c on two virtual STM32H743s; the CAN controller
 * adapter is board_mcan.c. This is a *partial* BSW/MCAL integration:
 * no CanTp, PduR, Dcm, real Trampoline scheduler or H723 chip model yet.
 */
#include "CanIf.h"
#include "CanIf_Priv.h"
#include "board_mcan.h"

#ifndef SIM_ROLE
#error "Define SIM_ROLE=0 (sender) or SIM_ROLE=1 (receiver)"
#endif

static void app_rx_indication(PduIdType rxPdu, const PduInfoType *pdu);
static void app_tx_confirmation(PduIdType txPdu, Std_ReturnType result);

#if SIM_ROLE == 1
/* Receiver: accept FD from 0x201, send Classical response 0x202. */
#define RX_IDENTIFIER 0x201u
#define TX_IDENTIFIER 0x202u
#else
/* Sender: send FD 0x201, accept the Classical response 0x202. */
#define RX_IDENTIFIER 0x202u
#define TX_IDENTIFIER (CAN_CANFD_ID_TYPE | 0x201u)
#endif

static const CanIf_RxPduType rx_table[] = {
    {.rxInd = app_rx_indication, .rxPduId = 0u, .canid = RX_IDENTIFIER,
     .mask = CAN_CANID_MASK, .hoh = 0u},
};
static const CanIf_TxPduType tx_table[] = {
    {.txConfirm = app_tx_confirmation, .txPduId = 0u, .canid = TX_IDENTIFIER,
     .p_canid = 0, .hoh = 0u, .ControllerId = 0u},
};
static CanIf_CtrlContextType ctrl_context[1];
static const CanIf_CtrlConfigType ctrl_config[] = {
    {.rxPdus = rx_table, .numOfRxPdus = 1u},
};
/* Real upstream CanIf.c resolves this external configuration. */
const CanIf_ConfigType CanIf_Config = {
    .txPdus = tx_table,
    .CtrlContexts = ctrl_context,
    .CtrlConfigs = ctrl_config,
    .numOfTxPdus = 1u,
    .numOfCtrls = 1u,
};

static const uint8_t request_fd[16] = {
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};
static const uint8_t classic_reply[8] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
};

static void app_rx_indication(PduIdType rxPdu, const PduInfoType *pdu)
{
    volatile uint32_t *diag = MCU_DEBUG;
    diag[7] += 1u; /* Actual upstream CanIf_RxIndication routing occurred. */
    if(rxPdu != 0u || pdu == 0 || pdu->SduDataPtr == 0) {
        diag[0] = MCU_STATUS_ERROR;
        return;
    }
    unsigned ok = 1u;
#if SIM_ROLE == 1
    if(pdu->SduLength != sizeof(request_fd)) {
        ok = 0u;
    } else {
        for(unsigned i = 0u; i < sizeof(request_fd); ++i) {
            if(pdu->SduDataPtr[i] != request_fd[i]) { ok = 0u; }
        }
    }
    if((diag[5] & (3u << 20)) != (3u << 20)) { ok = 0u; } /* BRS+FDF */
    if(ok) {
        diag[0] = MCU_STATUS_FD_ACCEPTED;
        PduInfoType reply = {.SduDataPtr = (uint8_t *)classic_reply,
                             .SduLength = sizeof(classic_reply), .MetaDataPtr = 0};
        if(CanIf_Transmit(0u, &reply) != E_OK) { diag[0] = MCU_STATUS_ERROR; }
    }
#else
    if(pdu->SduLength != sizeof(classic_reply)) {
        ok = 0u;
    } else {
        for(unsigned i = 0u; i < sizeof(classic_reply); ++i) {
            if(pdu->SduDataPtr[i] != classic_reply[i]) { ok = 0u; }
        }
    }
    if((diag[5] & (1u << 21)) != 0u) { ok = 0u; } /* Classical frame */
    if(ok) { diag[0] = MCU_STATUS_CLASSIC_ACCEPTED; }
#endif
    if(!ok) { diag[0] = MCU_STATUS_ERROR; }
}

static void app_tx_confirmation(PduIdType txPdu, Std_ReturnType result)
{
    volatile uint32_t *diag = MCU_DEBUG;
    if(txPdu == 0u && result == E_OK) { diag[8] += 1u; }
    else { diag[0] = MCU_STATUS_ERROR; }
}

int main(void)
{
    volatile uint32_t *diag = MCU_DEBUG;
    for(unsigned i = 0u; i < 12u; ++i) { diag[i] = 0u; }
    CanIf_Init(0);
    if(CanIf_SetControllerMode(0u, CAN_CS_STARTED) != E_OK ||
       CanIf_SetPduMode(0u, CANIF_ONLINE) != E_OK) {
        diag[0] = MCU_STATUS_ERROR;
        for(;;) { __asm__ volatile("wfi"); }
    }
#if SIM_ROLE == 1
    diag[0] = MCU_STATUS_START_RECEIVER;
#else
    diag[0] = MCU_STATUS_START_SENDER;
    PduInfoType request = {.SduDataPtr = (uint8_t *)request_fd,
                           .SduLength = sizeof(request_fd), .MetaDataPtr = 0};
    if(CanIf_Transmit(0u, &request) != E_OK) {
        diag[0] = MCU_STATUS_ERROR;
    }
#endif
    for(;;) {
        mcan_board_poll();
        mcan_board_delay(1000u);
    }
}
