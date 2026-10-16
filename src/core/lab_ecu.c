#include "lab_internal.h"

#include <string.h>

void lab_init(LabEcu *ecu, LabTransmit transmit, void *context, bool fd_telemetry) {
    if (ecu == NULL) {
        return;
    }
    memset(ecu, 0, sizeof(*ecu));
    ecu->transmit = transmit;
    ecu->tx_context = context;
    ecu->diag_session = 1U; /* default diagnostic session */
    ecu->next_app_ms = 10U;
    ecu->next_com_ms = 100U;
    ecu->fd_telemetry = fd_telemetry;
}

/* Application runnable, deliberately small and hardware independent. */
void lab_application_run_10ms(LabEcu *ecu) {
    /* Saw-tooth velocity: 5 cm/s increase each runnable activation. */
    ecu->speed_cm_s = (uint16_t)((ecu->speed_cm_s + 5U) % 3000U);
}

/* Scheduler-like host dispatcher, NOT a preemptive AUTOSAR OS. */
void lab_tick(LabEcu *ecu, uint32_t now_ms) {
    if (ecu == NULL || now_ms < ecu->now_ms) {
        return;
    }
    ecu->now_ms = now_ms;
    unsigned runs = 0U;
    while (now_ms >= ecu->next_app_ms && runs++ < 100U) {
        lab_application_run_10ms(ecu);
        ecu->next_app_ms += 10U;
    }
    if (now_ms >= ecu->next_app_ms) {
        /* Bound catch-up following host stalls; real OS scheduling differs. */
        ecu->next_app_ms = now_ms + 10U;
    }
    if (now_ms >= ecu->next_com_ms) {
        ecu->next_com_ms = now_ms + 100U;
        lab_rte_com_publish_speed(ecu);
    }
    lab_cantp_main_function(ecu);
}

void lab_on_frame(LabEcu *ecu, const LabFrame *frame) {
    if (ecu == NULL || frame == NULL || frame->id != LAB_CAN_DIAG_RX_ID || frame->is_fd ||
        frame->len == 0U || frame->len > 8U) {
        return;
    }
    /* CanIf-like receive routing -> CanTp -> Dcm. */
    lab_cantp_rx_indication(ecu, frame);
}
