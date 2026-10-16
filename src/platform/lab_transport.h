#ifndef LAB_TRANSPORT_H
#define LAB_TRANSPORT_H

#include "lab.h"
#include <stdint.h>

typedef enum { LAB_TRANSPORT_UDP = 0, LAB_TRANSPORT_SOCKETCAN = 1 } LabBackend;

typedef struct {
    int fd;
    LabBackend backend;
} LabTransport;

int lab_transport_open(LabTransport *transport, const char *backend, const char *ifname,
                       uint16_t port);
int lab_transport_receive(LabTransport *transport, LabFrame *frame);
int lab_transport_transmit(void *context, const LabFrame *frame);
void lab_transport_close(LabTransport *transport);
#endif
