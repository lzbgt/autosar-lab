#include "lab_internal.h"

#include <string.h>

static void reset_sender(LabEcu *ecu) {
    ecu->tp_length = 0U;
    ecu->tp_sent = 0U;
    ecu->tp_wait_flow = false;
    ecu->tp_wait_frames = 0U;
}

int lab_cantp_transmit(LabEcu *ecu, const uint8_t *payload, size_t length) {
    if (ecu == NULL || payload == NULL || length == 0U || length > LAB_CAN_DATA_MAX ||
        ecu->tp_length != 0U) {
        return -1;
    }

    uint8_t frame[8] = {0U};
    if (length <= 7U) {
        frame[0] = (uint8_t)length; /* ISO-TP Single Frame PCI */
        memcpy(frame + 1U, payload, length);
        return lab_pdur_canif_transmit(ecu, LAB_CAN_DIAG_TX_ID, frame, 8U, false);
    }

    /* First Frame: 12-bit total length; six UDS payload bytes. */
    frame[0] = (uint8_t)(0x10U | ((length >> 8U) & 0x0FU));
    frame[1] = (uint8_t)length;
    memcpy(frame + 2U, payload, 6U);
    if (lab_pdur_canif_transmit(ecu, LAB_CAN_DIAG_TX_ID, frame, 8U, false) != 0) {
        return -1;
    }

    memcpy(ecu->tp_payload, payload, length);
    ecu->tp_length = (uint16_t)length;
    ecu->tp_sent = 6U;
    ecu->tp_sequence = 1U;
    ecu->tp_wait_flow = true;
    ecu->tp_deadline_ms = ecu->now_ms + LAB_TP_TIMEOUT_MS;
    return 0;
}

void lab_cantp_rx_indication(LabEcu *ecu, const LabFrame *frame) {
    const uint8_t pci = (uint8_t)(frame->data[0] >> 4U);
    if (pci == 0U) {
        const size_t uds_length = frame->data[0] & 0x0FU;
        if (uds_length == 0U || uds_length > 7U || uds_length > (size_t)(frame->len - 1U)) {
            return;
        }
        lab_dcm_request(ecu, &frame->data[1], uds_length);
        return;
    }
    if (pci != 3U || frame->len < 3U || ecu->tp_length == 0U || !ecu->tp_wait_flow) {
        return;
    }

    const uint8_t flow_status = frame->data[0] & 0x0FU;
    if (flow_status == 1U) { /* WAIT */
        if (++ecu->tp_wait_frames > 3U) {
            reset_sender(ecu);
        } else {
            ecu->tp_deadline_ms = ecu->now_ms + LAB_TP_TIMEOUT_MS;
        }
        return;
    }
    if (flow_status != 0U) { /* OVERFLOW or reserved */
        reset_sender(ecu);
        return;
    }

    const uint8_t stmin = frame->data[2];
    if (stmin > 0x7FU && (stmin < 0xF1U || stmin > 0xF9U)) {
        reset_sender(ecu);
        return;
    }
    /* For 100..900us codes, 1ms polling is a conservative approximation. */
    ecu->tp_stmin_ms = stmin >= 0xF1U ? 1U : stmin;
    ecu->tp_block_remaining = frame->data[1] == 0U ? UINT8_MAX : frame->data[1];
    ecu->tp_wait_flow = false;
    ecu->tp_due_ms = ecu->now_ms;
}

void lab_cantp_main_function(LabEcu *ecu) {
    if (ecu->tp_length == 0U) {
        return;
    }
    if (ecu->tp_wait_flow) {
        if (ecu->now_ms >= ecu->tp_deadline_ms) {
            reset_sender(ecu);
        }
        return;
    }
    if (ecu->now_ms < ecu->tp_due_ms) {
        return;
    }
    if (ecu->tp_sent >= ecu->tp_length) {
        reset_sender(ecu);
        return;
    }

    const size_t remaining = ecu->tp_length - ecu->tp_sent;
    const size_t count = remaining > 7U ? 7U : remaining;
    uint8_t frame[8] = {0U};
    frame[0] = (uint8_t)(0x20U | (ecu->tp_sequence & 0x0FU));
    memcpy(&frame[1], ecu->tp_payload + ecu->tp_sent, count);
    if (lab_pdur_canif_transmit(ecu, LAB_CAN_DIAG_TX_ID, frame, 8U, false) != 0) {
        reset_sender(ecu);
        return;
    }
    ecu->tp_sent = (uint16_t)(ecu->tp_sent + count);
    ecu->tp_sequence = (uint8_t)((ecu->tp_sequence + 1U) & 0x0FU);
    ecu->tp_due_ms = ecu->now_ms + ecu->tp_stmin_ms;
    if (ecu->tp_sent == ecu->tp_length) {
        reset_sender(ecu);
    } else if (--ecu->tp_block_remaining == 0U) {
        ecu->tp_wait_flow = true;
        ecu->tp_deadline_ms = ecu->now_ms + LAB_TP_TIMEOUT_MS;
    }
}
