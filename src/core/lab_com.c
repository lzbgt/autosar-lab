#include "lab_internal.h"

#include <string.h>

/* Mini COM packs one unsigned, little-endian vehicle-speed signal. */
void lab_rte_com_publish_speed(LabEcu *ecu) {
    uint8_t bytes[16] = {0U};
    bytes[0] = (uint8_t)(ecu->speed_cm_s & 0xFFU);
    bytes[1] = (uint8_t)(ecu->speed_cm_s >> 8);
    bytes[2] = ecu->alive_counter++;
    bytes[3] = ecu->diag_session;
    /* In the FD demonstration, the other twelve bytes are reserved. */
    size_t len = ecu->fd_telemetry ? 16U : 8U;
    (void)lab_pdur_canif_transmit(ecu, LAB_CAN_SPEED_ID, bytes, len, ecu->fd_telemetry);
}

/* Simplified PduR->CanIf handoff: no routing tables, DET or Tx confirmations. */
int lab_pdur_canif_transmit(LabEcu *ecu, uint32_t can_id, const uint8_t *payload, size_t len,
                            bool fd) {
    if (ecu == NULL || payload == NULL || ecu->transmit == NULL || len == 0U ||
        len > LAB_CAN_DATA_MAX || (!fd && len > 8U) ||
        (can_id != LAB_CAN_SPEED_ID && can_id != LAB_CAN_DIAG_TX_ID)) {
        return -1;
    }
    LabFrame frame = {0};
    frame.id = can_id;
    frame.len = (uint8_t)len;
    frame.is_fd = fd;
    frame.brs = fd;
    memcpy(frame.data, payload, len);
    int result = ecu->transmit(ecu->tx_context, &frame);
    if (result != 0) {
        ecu->failed_transmissions++;
    }
    return result;
}
