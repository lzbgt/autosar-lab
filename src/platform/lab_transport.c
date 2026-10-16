#include "lab_transport.h"
#include <arpa/inet.h>
#include <errno.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define LAB_WIRE_SIZE 72U
#define LAB_WIRE_FLAG_FD 1U
#define LAB_WIRE_FLAG_BRS 2U

static bool valid_fd_length(uint8_t len) {
    return len <= 8U || len == 12U || len == 16U || len == 20U || len == 24U || len == 32U ||
           len == 48U || len == 64U;
}

static void encode_wire(uint8_t wire[LAB_WIRE_SIZE], const LabFrame *f) {
    memset(wire, 0, LAB_WIRE_SIZE);
    wire[0] = (uint8_t)(f->id & 0xFFU);
    wire[1] = (uint8_t)((f->id >> 8U) & 0xFFU);
    wire[2] = (uint8_t)((f->id >> 16U) & 0xFFU);
    wire[3] = (uint8_t)((f->id >> 24U) & 0xFFU);
    wire[4] = f->len;
    wire[5] = (uint8_t)((f->is_fd ? LAB_WIRE_FLAG_FD : 0U) | (f->brs ? LAB_WIRE_FLAG_BRS : 0U));
    memcpy(&wire[8], f->data, f->len);
}

static int decode_wire(LabFrame *f, const uint8_t *wire, size_t received) {
    if (received != LAB_WIRE_SIZE) {
        return 0;
    }
    const uint32_t id = ((uint32_t)wire[0]) | ((uint32_t)wire[1] << 8U) |
                        ((uint32_t)wire[2] << 16U) | ((uint32_t)wire[3] << 24U);
    const bool fd = (wire[5] & LAB_WIRE_FLAG_FD) != 0U;
    if (id > CAN_SFF_MASK || wire[4] > LAB_CAN_DATA_MAX || (!fd && wire[4] > 8U) ||
        (fd && !valid_fd_length(wire[4])) || (wire[5] & ~3U) != 0U || wire[6] != 0U ||
        wire[7] != 0U) {
        return 0;
    }
    memset(f, 0, sizeof(*f));
    f->id = id;
    f->len = wire[4];
    f->is_fd = fd;
    f->brs = (wire[5] & LAB_WIRE_FLAG_BRS) != 0U;
    memcpy(f->data, wire + 8U, f->len);
    return 1;
}

int lab_transport_open(LabTransport *transport, const char *backend, const char *ifname,
                       uint16_t port) {
    if (transport == NULL || backend == NULL) {
        errno = EINVAL;
        return -1;
    }
    transport->fd = -1;
    if (strcmp(backend, "udp") == 0) {
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            return -1;
        }
        struct sockaddr_in local = {0};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        local.sin_port = htons(0U);
        struct sockaddr_in broker = {0};
        broker.sin_family = AF_INET;
        broker.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        broker.sin_port = htons(port);
        if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0 ||
            connect(fd, (struct sockaddr *)&broker, sizeof(broker)) < 0 ||
            send(fd, "R", 1U, 0) != 1) {
            int save = errno;
            (void)close(fd);
            errno = save;
            return -1;
        }
        transport->fd = fd;
        transport->backend = LAB_TRANSPORT_UDP;
        return 0;
    }
    if (strcmp(backend, "socketcan") != 0 || ifname == NULL) {
        errno = EINVAL;
        return -1;
    }
    unsigned int index = if_nametoindex(ifname);
    if (index == 0U) {
        errno = ENODEV;
        return -1;
    }
    int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        return -1;
    }
    int enable_fd = 1;
    struct can_filter filter = {.can_id = LAB_CAN_DIAG_RX_ID,
                                .can_mask = CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG};
    struct sockaddr_can address = {.can_family = AF_CAN, .can_ifindex = (int)index};
    if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_fd, sizeof(enable_fd)) < 0 ||
        setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter)) < 0 ||
        bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        int save = errno;
        (void)close(fd);
        errno = save;
        return -1;
    }
    transport->fd = fd;
    transport->backend = LAB_TRANSPORT_SOCKETCAN;
    return 0;
}

int lab_transport_transmit(void *context, const LabFrame *frame) {
    LabTransport *transport = context;
    if (transport == NULL || frame == NULL || transport->fd < 0 || frame->id > CAN_SFF_MASK ||
        frame->len > LAB_CAN_DATA_MAX || (!frame->is_fd && frame->len > 8U) ||
        (frame->is_fd && !valid_fd_length(frame->len))) {
        errno = EINVAL;
        return -1;
    }
    if (transport->backend == LAB_TRANSPORT_UDP) {
        uint8_t wire[LAB_WIRE_SIZE];
        encode_wire(wire, frame);
        return send(transport->fd, wire, sizeof(wire), 0) == (ssize_t)sizeof(wire) ? 0 : -1;
    }
    if (frame->is_fd) {
        struct canfd_frame data = {0};
        data.can_id = frame->id;
        data.len = frame->len;
        data.flags = frame->brs ? CANFD_BRS : 0U;
        memcpy(data.data, frame->data, frame->len);
        return write(transport->fd, &data, CANFD_MTU) == CANFD_MTU ? 0 : -1;
    }
    struct can_frame data = {0};
    data.can_id = frame->id;
    data.can_dlc = frame->len;
    memcpy(data.data, frame->data, frame->len);
    return write(transport->fd, &data, CAN_MTU) == CAN_MTU ? 0 : -1;
}

int lab_transport_receive(LabTransport *transport, LabFrame *frame) {
    if (transport == NULL || frame == NULL || transport->fd < 0) {
        errno = EINVAL;
        return -1;
    }
    if (transport->backend == LAB_TRANSPORT_UDP) {
        uint8_t wire[LAB_WIRE_SIZE];
        ssize_t got = recv(transport->fd, wire, sizeof(wire), 0);
        if (got < 0) {
            return -1;
        }
        return decode_wire(frame, wire, (size_t)got);
    }
    struct canfd_frame data = {0};
    ssize_t got = read(transport->fd, &data, CANFD_MTU);
    if (got < 0) {
        return -1;
    }
    if (got != CAN_MTU && got != CANFD_MTU) {
        return 0;
    }
    if ((data.can_id & (CAN_EFF_FLAG | CAN_ERR_FLAG | CAN_RTR_FLAG)) != 0U ||
        data.len > LAB_CAN_DATA_MAX || (got == CAN_MTU && data.len > 8U)) {
        return 0;
    }
    memset(frame, 0, sizeof(*frame));
    frame->id = data.can_id & CAN_SFF_MASK;
    frame->len = data.len;
    frame->is_fd = got == CANFD_MTU;
    frame->brs = frame->is_fd && (data.flags & CANFD_BRS) != 0U;
    memcpy(frame->data, data.data, frame->len);
    return 1;
}

void lab_transport_close(LabTransport *transport) {
    if (transport != NULL && transport->fd >= 0) {
        (void)close(transport->fd);
        transport->fd = -1;
    }
}
