#include "lab.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    LabFrame frames[64];
    size_t count;
    int fail;
} Sink;

static int collect(void *context, const LabFrame *frame) {
    Sink *sink = context;
    if (sink->fail || sink->count == 64U) {
        return -1;
    }
    sink->frames[sink->count++] = *frame;
    return 0;
}

static void request(LabEcu *ecu, uint8_t n, uint8_t sid, uint8_t b, uint8_t c) {
    LabFrame frame = {0};
    frame.id = LAB_CAN_DIAG_RX_ID;
    frame.len = 8U;
    frame.data[0] = n;
    frame.data[1] = sid;
    frame.data[2] = b;
    frame.data[3] = c;
    lab_on_frame(ecu, &frame);
}

int main(void) {
    Sink sink = {0};
    LabEcu ecu;
    lab_init(&ecu, collect, &sink, false);
    lab_tick(&ecu, 100U);
    assert(sink.count == 1U);
    assert(sink.frames[0].id == LAB_CAN_SPEED_ID);
    assert(sink.frames[0].len == 8U);
    assert(!sink.frames[0].is_fd);
    assert(sink.frames[0].data[0] == 50U);
    assert(sink.frames[0].data[1] == 0U);
    assert(sink.frames[0].data[2] == 0U);

    request(&ecu, 2U, 0x10U, 3U, 0U);
    assert(sink.count == 2U);
    assert(sink.frames[1].id == LAB_CAN_DIAG_TX_ID);
    assert(sink.frames[1].data[0] == 6U);
    assert(sink.frames[1].data[1] == 0x50U);
    assert(ecu.diag_session == 3U);

    request(&ecu, 3U, 0x22U, 0xF1U, 0x87U);
    assert(sink.count == 3U);
    assert(sink.frames[2].data[0] == 7U);
    assert(sink.frames[2].data[1] == 0x62U);
    assert(sink.frames[2].data[4] == 'L');

    request(&ecu, 3U, 0x22U, 0xF1U, 0x90U);
    assert(sink.count == 4U);
    assert(sink.frames[3].data[0] == 0x10U);
    assert(sink.frames[3].data[1] == 20U);
    assert(sink.frames[3].data[2] == 0x62U);
    lab_tick(&ecu, 101U);
    assert(sink.count == 4U); /* No CF before flow control. */

    LabFrame fc = {0};
    fc.id = LAB_CAN_DIAG_RX_ID;
    fc.len = 8U;
    fc.data[0] = 0x30U;
    fc.data[1] = 1U; /* Block size = one CF */
    fc.data[2] = 0U;
    lab_on_frame(&ecu, &fc);
    lab_tick(&ecu, 102U);
    assert(sink.count == 5U && sink.frames[4].data[0] == 0x21U);
    lab_tick(&ecu, 103U);
    assert(sink.count == 5U); /* Need a second FC after BS exhausted. */
    lab_on_frame(&ecu, &fc);
    lab_tick(&ecu, 104U);
    assert(sink.count == 6U && sink.frames[5].data[0] == 0x22U);
    assert(ecu.tp_length == 0U);

    request(&ecu, 2U, 0x10U, 0x7FU, 0U);
    assert(sink.count == 7U);
    assert(sink.frames[6].data[1] == 0x7FU);
    assert(sink.frames[6].data[2] == 0x10U);
    assert(sink.frames[6].data[3] == 0x12U);

    request(&ecu, 3U, 0x22U, 0xF1U, 0x90U);
    assert(sink.count == 8U && ecu.tp_wait_flow);
    lab_tick(&ecu, 1105U);
    assert(ecu.tp_length == 0U); /* Timeout, clean transport state. */

    Sink fd_sink = {0};
    LabEcu fd_ecu;
    lab_init(&fd_ecu, collect, &fd_sink, true);
    lab_tick(&fd_ecu, 100U);
    assert(fd_sink.count == 1U);
    assert(fd_sink.frames[0].is_fd && fd_sink.frames[0].brs);
    assert(fd_sink.frames[0].len == 16U);

    Sink error_sink = {.fail = 1};
    LabEcu failing;
    lab_init(&failing, collect, &error_sink, false);
    lab_tick(&failing, 100U);
    assert(failing.failed_transmissions == 1U);

    puts("PASS: periodic runnable, COM, UDS, ISO-TP FC/BS, timeout, CAN FD and errors");
    return 0;
}
