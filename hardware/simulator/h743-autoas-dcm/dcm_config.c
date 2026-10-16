/* Real AutoAS Diagnostic Communication Manager configuration; no regenerated
 * host application's broad/unsafe demo services. This is intentionally
 * handwritten, not proof of AUTOSAR ARXML configuration-tool conformance.
 */
#include "Dcm.h"
#include "Dcm_Cfg.h"
#include "Dcm_Priv.h"
#include "board_mcan.h"

static uint8_t diagnostic_rx[64];
static uint8_t diagnostic_tx[64];
static Dcm_ReadDIDContextType did_context;
static const uint8_t allowed_sessions[] = {
    DCM_DEFAULT_SESSION, DCM_EXTENDED_DIAGNOSTIC_SESSION
};
static Std_ReturnType allow_session_change(Dcm_SesCtrlType current,
                                            Dcm_SesCtrlType requested,
                                            Dcm_NegativeResponseCodeType *nrc)
{
    (void)current; (void)requested; (void)nrc;
    return E_OK;
}
static const Dcm_SessionControlConfigType session_config = {
    .GetSesChgPermissionFnc = allow_session_change,
    .sesCtrls = allowed_sessions,
    .numOfSesCtrls = 2u,
};

Std_ReturnType Demo_ReadFingerPrint(Dcm_OpStatusType status,
                                   uint8_t *data, uint16_t length,
                                   Dcm_NegativeResponseCodeType *nrc)
{
    (void)status;
    if(data == 0 || nrc == 0 || length != 10u) {
        if(nrc != 0) { *nrc = DCM_E_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT; }
        return E_NOT_OK;
    }
    for(unsigned i = 0u; i < 10u; ++i) { data[i] = 0xA5u; }
    MCU_DEBUG[7] += 1u; /* Actual upstream Dcm called real DID callback. */
    return E_OK;
}
void Dcm_SessionChangeIndication(Dcm_SesCtrlType current,
                                  Dcm_SesCtrlType requested,
                                  boolean timeout)
{
    (void)current; (void)requested; (void)timeout;
    MCU_DEBUG[15] += 1u; /* Separate from M_CAN raw frame counters [11..14]. */
}

static const Dcm_rDIDConfigType fingerprint = {
    .id = 0xF15Bu,
    .length = 10u,
    .readDIdFnc = Demo_ReadFingerPrint,
    .SesSecAccess = {.sessionMask = DCM_DFTS_MASK | DCM_EXTDS_MASK,
                     .miscMask = DCM_MISC_PHYSICAL},
};
static const Dcm_ReadDIDType read_dids[] = {{
    .context = &did_context,
    .rDID = &fingerprint,
}};
static const Dcm_ReadDIDConfigType read_did_config = {
    .DIDs = read_dids, .numOfDIDs = 1u,
};
static const Dcm_ServiceType services[] = {
    {
        .SID = SID_DIAGNOSTIC_SESSION_CONTROL,
        .SesSecAccess = {.sessionMask = DCM_DFTS_MASK | DCM_EXTDS_MASK,
                         .miscMask = DCM_MISC_PHYSICAL | DCM_MISC_SUB_FUNCTION},
        .dspServiceFnc = Dcm_DspSessionControl,
        .config = &session_config,
    },
    {
        .SID = SID_READ_DATA_BY_IDENTIFIER,
        .SesSecAccess = {.sessionMask = DCM_DFTS_MASK | DCM_EXTDS_MASK,
                         .miscMask = DCM_MISC_PHYSICAL},
        .dspServiceFnc = Dcm_DspReadDataByIdentifier,
        .config = &read_did_config,
    },
};
static const Dcm_ServiceTableType service_table = {
    .services = services,
    .numOfServices = 2u,
};
static const Dcm_ServiceTableType *const service_tables[] = {
    &service_table,
};
static const Dcm_TimingConfigType timing = {
    .S3Server = DCM_CONVERT_MS_TO_MAIN_CYCLES(5000u),
    .P2ServerAdjust = DCM_CONVERT_MS_TO_MAIN_CYCLES(20u),
    .P2StarServerAdjust = DCM_CONVERT_MS_TO_MAIN_CYCLES(20u),
    .P2ServerMax = DCM_CONVERT_MS_TO_MAIN_CYCLES(50u),
    .P2StarServerMax = DCM_CONVERT_MS_TO_MAIN_CYCLES(5000u),
};
static const Dcm_DslDiagRespConfigType diagnostic_response = {
    .MaxNumRespPend = 3u,
};
static const Dcm_ChannelType channel = {
    .TxPduId = 1u, /* Real PduR Tx path index, to genuine CanTp_Transmit. */
    .reqType = DCM_PHYSICAL_REQUEST,
};
const Dcm_ConfigType Dcm_Config = {
    .ServiceVerificationFnc = 0,
    .rxBuffer = diagnostic_rx,
    .txBuffer = diagnostic_tx,
    .rxBufferSize = sizeof(diagnostic_rx),
    .txBufferSize = sizeof(diagnostic_tx),
    .channles = &channel,
    .numOfChls = 1u,
    .serviceTables = service_tables,
    .numOfServiceTables = 1u,
    .timing = &timing,
    .dslDisgResp = &diagnostic_response,
    .rDIDs = &fingerprint,
    .numOfrDIDs = 1u,
};
