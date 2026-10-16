/* STM32H743 Renode ECU: actual upstream AutoAS CanIf, CanTp, PduR and Dcm.
 *
 * The test application owns only static configuration and a single 10-byte
 * fingerprint callback. There is NO handwritten diagnostic-service parser
 * on this ECU. See dcm_config.c for enabled services and safety boundaries.
 */
#include "CanIf.h"
#include "CanIf_Priv.h"
#include "CanTp.h"
#include "CanTp_Priv.h"
#include "PduR.h"
#undef DET_THIS_MODULE_ID
#include "PduR_Priv.h"
#include "Dcm.h"
#undef DET_THIS_MODULE_ID
#include "Dcm_Priv.h"
#include "board_mcan.h"
#include <stddef.h>
#include <stdint.h>

#define SERVER_PASS_DID 0xCAFE22DCu
#define SERVER_PASS_SESSION 0xCAFE1003u
#define SERVER_PASS_REJECTION 0xCAFE2231u
#define SERVER_ERROR 0xBAD200DCu

static uint8_t tp_data[64];
static CanTp_ChannelContextType tp_context[1];
static const CanTp_ChannelConfigType tp_channel = {
    .data = tp_data, .AddressingFormat = CANTP_STANDARD,
    .CanIfTxPduId = 0u, .PduR_RxPduId = 0u, .PduR_TxPduId = 1u,
    .N_As = 100u, .N_Bs = 100u, .N_Cr = 100u,
    .STmin = 0u, .BS = 0u, .N_TA = 0u, .CanTpRxWftMax = 3u,
    .LL_DL = 8u, .padding = 0u, .comType = CANTP_PHYSICAL,
};
const CanTp_ConfigType CanTp_Config = {
    .channelConfigs = &tp_channel, .channelContexts = tp_context,
    .numOfChannels = 1u,
};
static void frame_transmitted(PduIdType id, Std_ReturnType result)
{
    MCU_DEBUG[8]++; /* Hardware-level transmit confirmation count. */
    CanTp_TxConfirmation(id, result);
}
static const CanIf_RxPduType rx_table[] = {{
    .rxInd = CanTp_RxIndication, .rxPduId = 0u, .canid = 0x731u,
    .mask = CAN_CANID_MASK, .hoh = 0u,
}};
static const CanIf_TxPduType tx_table[] = {{
    .txConfirm = frame_transmitted, .txPduId = 0u, .canid = 0x732u,
    .p_canid = 0, .hoh = 0u, .ControllerId = 0u,
}};
static CanIf_CtrlContextType ctrl_context[1];
static const CanIf_CtrlConfigType ctrl_config[] = {{
    .rxPdus = rx_table, .numOfRxPdus = 1u,
}};
const CanIf_ConfigType CanIf_Config = {
    .txPdus = tx_table, .CtrlContexts = ctrl_context,
    .CtrlConfigs = ctrl_config, .numOfTxPdus = 1u, .numOfCtrls = 1u,
};

/* Genuine AutoAS PduR routing tables: Dcm<->CanTp, no gateway/pools. */
static const PduR_ApiType dcm_api = {
    .StartOfReception = Dcm_StartOfReception,
    .CopyRxData = Dcm_CopyRxData,
    .TpRxIndication = Dcm_TpRxIndication,
    .CopyTxData = Dcm_CopyTxData,
    .TxConfirmation = Dcm_TpTxConfirmation,
};
static const PduR_ApiType cantp_api = {.Transmit = CanTp_Transmit};
static const PduR_PduType path_rx_src = {
    .Module = PDUR_MODULE_CANTP, .PduHandleId = 0u, .api = &cantp_api,
};
static const PduR_PduType path_rx_dst[] = {{
    .Module = PDUR_MODULE_DCM, .PduHandleId = 0u, .api = &dcm_api,
}};
static const PduR_PduType path_tx_src = {
    .Module = PDUR_MODULE_DCM, .PduHandleId = 0u, .api = &dcm_api,
};
static const PduR_PduType path_tx_dst[] = {{
    .Module = PDUR_MODULE_CANTP, .PduHandleId = 0u, .api = &cantp_api,
}};
static const PduR_RoutingPathType paths[] = {
    {.SrcPduRef = &path_rx_src, .DestPduRef = path_rx_dst,
     .numOfDestPdus = 1u},
    {.SrcPduRef = &path_tx_src, .DestPduRef = path_tx_dst,
     .numOfDestPdus = 1u},
};
const PduR_ConfigType PduR_Config = {
    .RoutingPaths = paths, .numOfRoutingPaths = 2u,
};

int main(void)
{
    volatile uint32_t *const diag = MCU_DEBUG;
    for(unsigned i = 0u; i < 16u; ++i) { diag[i] = 0u; }

    CanIf_Init(0);
    PduR_Init(0);
    CanTp_Init(0);
    Dcm_Init(0);
    if(CanIf_SetControllerMode(0u, CAN_CS_STARTED) != E_OK ||
       CanIf_SetPduMode(0u, CANIF_ONLINE) != E_OK) {
        diag[0] = SERVER_ERROR;
        for(;;) { __asm__ volatile("wfi"); }
    }
    diag[0] = 0x70000004u;
    unsigned iteration = 0u;
    for(;;) {
        mcan_board_poll();
        CanTp_MainFunction_Fast();
        if(++iteration == 5000u) {
            iteration = 0u;
            CanTp_MainFunction();
            Dcm_MainFunction();
        }
        /* Mark success only after real Dcm response transport has confirmed.
         * Exact contents and ISO-TP framing are checked independently on
         * the tester MCU. No success marker is based on transmit-request alone. */
        Dcm_ContextType *context = Dcm_GetContext();
        if(diag[8] >= 1u && diag[9] == 0u &&
           context->txBufferState == DCM_BUFFER_IDLE &&
           context->rxBufferState == DCM_BUFFER_IDLE) {
            if(diag[7] > 0u && diag[8] >= 2u) {
                diag[0] = SERVER_PASS_DID;
                diag[10] = 1u;
            } else if(diag[15] > 0u && diag[8] >= 1u) {
                diag[0] = SERVER_PASS_SESSION;
                diag[10] = 1u;
            } else if(diag[7] == 0u && diag[15] == 0u &&
                      context->currentSID == 0x22u) {
                diag[0] = SERVER_PASS_REJECTION;
                diag[10] = 1u;
            }
        }
        mcan_board_delay(1000u);
    }
}
void *memset(void *dst, int value, size_t n)
{
    uint8_t *p = (uint8_t *)dst;
    for(size_t i = 0; i < n; ++i) { p[i] = (uint8_t)value; }
    return dst;
}
void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *p = (uint8_t *)dst;
    const uint8_t *q = (const uint8_t *)src;
    for(size_t i = 0; i < n; ++i) { p[i] = q[i]; }
    return dst;
}
