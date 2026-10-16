/* Educational ECU API: no AUTOSAR API or conformity is implied. */
#ifndef LAB_H
#define LAB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LAB_CAN_SPEED_ID UINT32_C(0x201)
#define LAB_CAN_DIAG_RX_ID UINT32_C(0x7E0)
#define LAB_CAN_DIAG_TX_ID UINT32_C(0x7E8)
#define LAB_CAN_DATA_MAX 64U
#define LAB_TP_TIMEOUT_MS 1000U

typedef struct {
    uint32_t id; /* 11-bit CAN identifier in these labs */
    uint8_t len; /* payload bytes, not raw DLC */
    bool is_fd;
    bool brs;
    uint8_t data[LAB_CAN_DATA_MAX];
} LabFrame;

typedef int (*LabTransmit)(void *context, const LabFrame *frame);

typedef struct {
    LabTransmit transmit;
    void *tx_context;
    uint32_t now_ms;
    uint32_t next_app_ms;
    uint32_t next_com_ms;
    uint16_t speed_cm_s;
    uint8_t alive_counter;
    uint8_t diag_session;
    bool fd_telemetry;
    uint32_t failed_transmissions;

    /* Single ISO-TP sender. Incoming multi-frame requests not supported. */
    uint8_t tp_payload[LAB_CAN_DATA_MAX];
    uint16_t tp_length;
    uint16_t tp_sent;
    uint8_t tp_sequence;
    uint8_t tp_block_remaining;
    uint8_t tp_stmin_ms;
    uint8_t tp_wait_frames;
    bool tp_wait_flow;
    uint32_t tp_due_ms;
    uint32_t tp_deadline_ms;
} LabEcu;

void lab_init(LabEcu *ecu, LabTransmit transmit, void *context, bool fd_telemetry);
/* now_ms: milliseconds since boot; call repeatedly, including while idle. */
void lab_tick(LabEcu *ecu, uint32_t now_ms);
/* Caller owns the frame. Unexpected IDs and malformed requests are ignored. */
void lab_on_frame(LabEcu *ecu, const LabFrame *frame);

#endif
