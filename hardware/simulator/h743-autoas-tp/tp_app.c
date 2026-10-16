/* Two-node educational UDS-over-ISO-TP test with REAL upstream AutoAS
 * CanIf.c, CanTp.c, PduR.c, PduR_CanTp.c, and PduR_Dcm.c. The application
 * supplies only the diagnostic payload/buffer callbacks in place of Dcm.
 *
 * Tester sends UDS 22 F1 5B in a Single Frame at CAN ID 0x731.
 * Server replies with 62 F1 5B + 10 x A5 on 0x732, requiring First Frame,
 * Flow Control, and Consecutive Frame(s). This loop is not a real-time OS.
 */
#include "CanIf.h"
#include "CanIf_Priv.h"
#include "CanTp.h"
#include "CanTp_Priv.h"
#include "PduR_CanTp.h"
#include "PduR.h"
/* Each upstream module defines its own development-error module identity. */
#undef DET_THIS_MODULE_ID
#include "PduR_Priv.h"
#include "PduR_Dcm.h"
#include "board_mcan.h"
#include <stddef.h>
#include <stdint.h>

#ifndef SIM_ROLE
#error "Define SIM_ROLE=0 (tester) or SIM_ROLE=1 (server)"
#endif

#define STATUS_PASS_TESTER 0xCAFE220Du
#define STATUS_PASS_SERVER 0xCAFE220Eu
#define STATUS_BAD_TP 0xBAD20001u

static uint8_t tp_data[64];
static CanTp_ChannelContextType tp_ctx[1];
static const CanTp_ChannelConfigType tp_channels[1] = {
    {
        .data = tp_data,
        .AddressingFormat = CANTP_STANDARD,
        .CanIfTxPduId = 0u,
        .PduR_RxPduId = 0u,
        .PduR_TxPduId = 1u,
        .N_As = 100u, .N_Bs = 100u, .N_Cr = 100u,
        .STmin = 0u, .BS = 0u, .N_TA = 0u, .CanTpRxWftMax = 3u,
        .LL_DL = 8u, .padding = 0u, .comType = CANTP_PHYSICAL,
    },
};
const CanTp_ConfigType CanTp_Config = {
    .channelConfigs = tp_channels,
    .channelContexts = tp_ctx,
    .numOfChannels = 1u,
};
static void confirm_to_cantp(PduIdType id, Std_ReturnType status);

#if SIM_ROLE == 1
#define RX_IDENTIFIER 0x731u
#define TX_IDENTIFIER 0x732u
static const uint8_t tx_payload[] = {
    0x62, 0xF1, 0x5B,
    0xA5, 0xA5, 0xA5, 0xA5, 0xA5,
    0xA5, 0xA5, 0xA5, 0xA5, 0xA5,
};
#else
#define RX_IDENTIFIER 0x732u
#define TX_IDENTIFIER 0x731u
static const uint8_t tx_payload[] = {0x22, 0xF1, 0x5B};
#endif

static const CanIf_RxPduType rx_table[1] = {
    {.rxInd = CanTp_RxIndication, .rxPduId = 0u,
     .canid = RX_IDENTIFIER, .mask = CAN_CANID_MASK, .hoh = 0u},
};
static const CanIf_TxPduType tx_table[1] = {
    {.txConfirm = confirm_to_cantp, .txPduId = 0u, .canid = TX_IDENTIFIER,
     .p_canid = 0, .hoh = 0u, .ControllerId = 0u},
};
static CanIf_CtrlContextType ctrl_ctx[1];
static const CanIf_CtrlConfigType ctrl_config[] = {
    {.rxPdus = rx_table, .numOfRxPdus = 1u},
};
const CanIf_ConfigType CanIf_Config = {
    .txPdus = tx_table, .CtrlContexts = ctrl_ctx,
    .CtrlConfigs = ctrl_config, .numOfTxPdus = 1u, .numOfCtrls = 1u,
};
static uint8_t rx_buffer[64];
static PduLengthType rx_length;
static PduLengthType rx_received;
static volatile uint8_t pending_service_response;
#if SIM_ROLE == 0
static unsigned tx_started;
#endif
static unsigned tp_txs_completed;

static void confirm_to_cantp(PduIdType id, Std_ReturnType status)
{
    MCU_DEBUG[8]++; /* Physical frame confirmations routed by actual CanIf */
    CanTp_TxConfirmation(id, status);
}

/* Application callbacks are invoked by genuine upstream PduR.
 * This is a test-only diagnostic service, NOT actual AutoAS Dcm. */
BufReq_ReturnType App_StartOfReception(PduIdType id,
                                              const PduInfoType *info,
                                              PduLengthType total,
                                              PduLengthType *free_size)
{
    (void)info;
    if(id != 0u || free_size == 0 || total > sizeof(rx_buffer) || total == 0u) {
        return BUFREQ_E_OVFL;
    }
    rx_length = total;
    rx_received = 0u;
    *free_size = sizeof(rx_buffer);
    return BUFREQ_OK;
}
BufReq_ReturnType App_CopyRxData(PduIdType id, const PduInfoType *info,
                                        PduLengthType *free_size)
{
    if(id != 0u || info == 0 || info->SduDataPtr == 0 || free_size == 0 ||
       rx_received + info->SduLength > rx_length ||
       rx_received + info->SduLength > sizeof(rx_buffer)) {
        return BUFREQ_E_OVFL;
    }
    for(PduLengthType i = 0; i < info->SduLength; ++i) {
        rx_buffer[rx_received + i] = info->SduDataPtr[i];
    }
    rx_received += info->SduLength;
    *free_size = sizeof(rx_buffer) - rx_received;
    return BUFREQ_OK;
}
void App_RxIndication(PduIdType id, Std_ReturnType result)
{
    if(id != 0u || result != E_OK || rx_received != rx_length) {
        MCU_DEBUG[0] = STATUS_BAD_TP;
        return;
    }
    MCU_DEBUG[7]++; /* Fully reassembled UDS application PDU */
#if SIM_ROLE == 1
    if(rx_length == 3u && rx_buffer[0] == 0x22 && rx_buffer[1] == 0xF1 &&
       rx_buffer[2] == 0x5B) {
        pending_service_response = 1u;
    } else {
        MCU_DEBUG[0] = STATUS_BAD_TP;
    }
#else
    if(rx_length == sizeof(tx_payload) + 10u &&
       rx_buffer[0] == 0x62 && rx_buffer[1] == 0xF1 && rx_buffer[2] == 0x5B) {
        unsigned valid = 1u;
        for(unsigned i = 3u; i < 13u; ++i) {
            if(rx_buffer[i] != 0xA5u) { valid = 0u; }
        }
        MCU_DEBUG[0] = valid ? STATUS_PASS_TESTER : STATUS_BAD_TP;
    } else {
        MCU_DEBUG[0] = STATUS_BAD_TP;
    }
#endif
}
BufReq_ReturnType App_CopyTxData(PduIdType id, const PduInfoType *info,
                                        const RetryInfoType *retry,
                                        PduLengthType *available_data)
{
    (void)retry;
    if(id != 0u || info == 0 || info->SduDataPtr == 0 ||
       info->MetaDataPtr == 0 || available_data == 0) {
        return BUFREQ_E_NOT_OK;
    }
    const PduLengthType offset = *(const PduLengthType *)info->MetaDataPtr;
    if(offset > sizeof(tx_payload) ||
       info->SduLength > sizeof(tx_payload) - offset) {
        return BUFREQ_E_NOT_OK;
    }
    for(PduLengthType i = 0; i < info->SduLength; ++i) {
        info->SduDataPtr[i] = tx_payload[offset + i];
    }
    *available_data = sizeof(tx_payload) - offset - info->SduLength;
    return BUFREQ_OK;
}
void App_TxConfirmation(PduIdType id, Std_ReturnType result)
{
    if(id == 0u && result == E_OK) {
        tp_txs_completed++;
        MCU_DEBUG[10] = tp_txs_completed;
#if SIM_ROLE == 1
        MCU_DEBUG[0] = STATUS_PASS_SERVER;
#endif
    } else {
        MCU_DEBUG[0] = STATUS_BAD_TP;
    }
}

/* Two real upstream Protocol Data Unit Router paths. Only the terminal
 * application callbacks are teaching substitutes for Diagnostic Communication
 * Manager (Dcm); no Dcm.c or commercial RTE is linked. */
static const PduR_ApiType app_api = {
    .StartOfReception = App_StartOfReception,
    .CopyRxData = App_CopyRxData,
    .TpRxIndication = App_RxIndication,
    .CopyTxData = App_CopyTxData,
    .TxConfirmation = App_TxConfirmation,
};
static const PduR_ApiType transport_api = {.Transmit = CanTp_Transmit};
static const PduR_PduType path_rx_source = {
    .Module = PDUR_MODULE_CANTP, .PduHandleId = 0u, .api = &transport_api,
};
static const PduR_PduType path_rx_destination[1] = {{
    .Module = PDUR_MODULE_DCM, .PduHandleId = 0u, .api = &app_api,
}};
static const PduR_PduType path_tx_source = {
    .Module = PDUR_MODULE_DCM, .PduHandleId = 0u, .api = &app_api,
};
static const PduR_PduType path_tx_destination[1] = {{
    .Module = PDUR_MODULE_CANTP, .PduHandleId = 0u, .api = &transport_api,
}};
static const PduR_RoutingPathType routes[2] = {
    {.SrcPduRef = &path_rx_source, .DestPduRef = path_rx_destination,
     .numOfDestPdus = 1u},
    {.SrcPduRef = &path_tx_source, .DestPduRef = path_tx_destination,
     .numOfDestPdus = 1u},
};
const PduR_ConfigType PduR_Config = {
    .RoutingPaths = routes, .numOfRoutingPaths = 2u,
};

int main(void)
{
    for(unsigned i = 0u; i < 12u; ++i) { MCU_DEBUG[i] = 0u; }
    CanIf_Init(0);
    PduR_Init(0);
    CanTp_Init(0);
    if(CanIf_SetControllerMode(0u, CAN_CS_STARTED) != E_OK ||
       CanIf_SetPduMode(0u, CANIF_ONLINE) != E_OK) {
        MCU_DEBUG[0] = STATUS_BAD_TP;
        for(;;) { __asm__ volatile("wfi"); }
    }
#if SIM_ROLE == 1
    MCU_DEBUG[0] = 0x70000003u;
#else
    MCU_DEBUG[0] = 0x50000003u;
#endif
    unsigned loop_counter = 0u;
    for(;;) {
        mcan_board_poll();
        CanTp_MainFunction_Fast();
        if(++loop_counter == 5000u) {
            loop_counter = 0u;
            CanTp_MainFunction();
        }
#if SIM_ROLE == 0
        if(!tx_started && MCU_DEBUG[6] == 0u && ++tx_started == 1u) {
            const PduInfoType pdu = {
                .SduDataPtr = (uint8_t *)tx_payload,
                .SduLength = sizeof(tx_payload), .MetaDataPtr = 0
            };
            if(PduR_DcmTransmit(1u, &pdu) != E_OK) {
                MCU_DEBUG[0] = STATUS_BAD_TP;
            }
        }
#else
        if(pending_service_response) {
            PduInfoType reply = {
                .SduDataPtr = (uint8_t *)tx_payload,
                .SduLength = sizeof(tx_payload), .MetaDataPtr = 0
            };
            if(PduR_DcmTransmit(1u, &reply) == E_OK) {
                pending_service_response = 0u;
            }
        }
#endif
        mcan_board_delay(1000u);
    }
}

/* Zero-filling and buffer copies used by unmodified AutoAS CanTp.c.
 * Freestanding self-contained code, with no dynamic allocation. */
void *memset(void *dest, int value, size_t size)
{
    uint8_t *p = (uint8_t *)dest;
    for(size_t i = 0u; i < size; ++i) { p[i] = (uint8_t)value; }
    return dest;
}
void *memcpy(void *dest, const void *source, size_t size)
{
    uint8_t *p = (uint8_t *)dest;
    const uint8_t *q = (const uint8_t *)source;
    for(size_t i = 0u; i < size; ++i) { p[i] = q[i]; }
    return dest;
}
