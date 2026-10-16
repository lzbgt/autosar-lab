#include "lab_internal.h"

#include <string.h>

/* Tutorial-only UDS responder, NOT a complete ISO 14229 DCM implementation. */
static void negative(LabEcu *ecu, uint8_t sid, uint8_t nrc) {
    const uint8_t response[] = {0x7FU, sid, nrc};
    (void)lab_cantp_transmit(ecu, response, sizeof(response));
}

void lab_dcm_request(LabEcu *ecu, const uint8_t *request, size_t len) {
    if (ecu == NULL || request == NULL || len == 0U) {
        return;
    }

    switch (request[0]) {
    case 0x10U: { /* DiagnosticSessionControl */
        if (len != 2U) {
            negative(ecu, 0x10U, 0x13U); /* incorrectMessageLengthOrInvalidFormat */
            return;
        }
        const uint8_t session = request[1];
        if (session != 1U && session != 3U) {
            negative(ecu, 0x10U, 0x12U); /* subFunctionNotSupported */
            return;
        }
        ecu->diag_session = session;
        const uint8_t response[] = {0x50U, session, 0x00U, 0x32U, 0x01U, 0xF4U};
        (void)lab_cantp_transmit(ecu, response, sizeof(response));
        return;
    }
    case 0x22U: { /* ReadDataByIdentifier */
        if (len != 3U) {
            negative(ecu, 0x22U, 0x13U);
            return;
        }
        const uint16_t did = (uint16_t)(((uint16_t)request[1] << 8) | request[2]);
        if (did == 0xF187U) { /* Educational version string: exactly 7-byte UDS PDU */
            const uint8_t response[] = {0x62U, 0xF1U, 0x87U, 'L', 'A', 'B', '1'};
            (void)lab_cantp_transmit(ecu, response, sizeof(response));
        } else if (did == 0xF190U) { /* 17-character fake VIN: multi-frame */
            const char vin[] = "LAB00000000000001";
            uint8_t response[3U + 17U] = {0x62U, 0xF1U, 0x90U};
            memcpy(response + 3U, vin, 17U);
            (void)lab_cantp_transmit(ecu, response, sizeof(response));
        } else {
            negative(ecu, 0x22U, 0x31U); /* requestOutOfRange */
        }
        return;
    }
    default:
        negative(ecu, request[0], 0x11U); /* serviceNotSupported */
        return;
    }
}
