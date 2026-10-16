#define _POSIX_C_SOURCE 200809L
#include "lab.h"
#include "lab_transport.h"

#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t g_stop = 0;
static bool g_verbose = false;

static void stop_handler(int signo) {
    (void)signo;
    g_stop = 1;
}

static uint64_t monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (uint64_t)ts.tv_sec * UINT64_C(1000) + (uint64_t)ts.tv_nsec / UINT64_C(1000000);
}

static void usage(const char *binary) {
    fprintf(stderr,
            "Usage: %s [--backend udp|socketcan] [--ifname vcan0] "
            "[--port 18765] [--duration SECONDS] [--fd] [--verbose]\n",
            binary);
}

static int transmit(void *context, const LabFrame *frame) {
    if (g_verbose) {
        printf("TX id=0x%03" PRIX32 " len=%u fd=%u data=", frame->id, frame->len, frame->is_fd);
        for (unsigned i = 0U; i < frame->len; ++i) {
            printf("%02X", frame->data[i]);
        }
        putchar('\n');
    }
    return lab_transport_transmit(context, frame);
}

int main(int argc, char **argv) {
    const char *backend = "udp";
    const char *ifname = "vcan0";
    uint16_t port = 18765U;
    unsigned duration = 0U;
    bool fd = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            backend = argv[++i];
        } else if (strcmp(argv[i], "--ifname") == 0 && i + 1 < argc) {
            ifname = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            long n = strtol(argv[++i], NULL, 10);
            if (n < 1 || n > 65535) {
                usage(argv[0]);
                return 2;
            }
            port = (uint16_t)n;
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            long n = strtol(argv[++i], NULL, 10);
            if (n < 0 || n > 3600) {
                usage(argv[0]);
                return 2;
            }
            duration = (unsigned)n;
        } else if (strcmp(argv[i], "--fd") == 0) {
            fd = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            g_verbose = true;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    LabTransport transport;
    if (lab_transport_open(&transport, backend, ifname, port) < 0) {
        fprintf(stderr, "Unable to open %s backend: %s\n", backend, strerror(errno));
        return 1;
    }
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);
    setvbuf(stdout, NULL, _IOLBF, 0);
    LabEcu ecu;
    lab_init(&ecu, transmit, &transport, fd);
    uint64_t started = monotonic_ms();
    uint32_t last_report = 0U;
    printf("ECU started: %s %s, telemetry=%s (educational, non-AUTOSAR)\n", backend,
           strcmp(backend, "udp") == 0 ? "loopback" : ifname,
           fd ? "CAN FD 16 bytes" : "CAN 2.0 8 bytes");
    while (!g_stop) {
        uint32_t now = (uint32_t)(monotonic_ms() - started);
        if (duration != 0U && now >= duration * 1000U) {
            break;
        }
        struct pollfd pfd = {.fd = transport.fd, .events = POLLIN};
        int ready = poll(&pfd, 1U, 10);
        if (ready < 0 && errno != EINTR) {
            perror("poll");
            lab_transport_close(&transport);
            return 1;
        }
        if (ready > 0 && (pfd.revents & POLLIN) != 0) {
            LabFrame incoming;
            int got = lab_transport_receive(&transport, &incoming);
            if (got < 0 && errno != EINTR) {
                perror("CAN receive");
                lab_transport_close(&transport);
                return 1;
            }
            if (got > 0) {
                lab_on_frame(&ecu, &incoming);
            }
        }
        now = (uint32_t)(monotonic_ms() - started);
        lab_tick(&ecu, now);
        if (now >= last_report + 1000U) {
            printf("t=%" PRIu32 "ms speed=%u cm/s session=%u tx_errors=%" PRIu32 "\n", now,
                   ecu.speed_cm_s, ecu.diag_session, ecu.failed_transmissions);
            last_report = now;
        }
    }
    lab_transport_close(&transport);
    return ecu.failed_transmissions == 0U ? 0 : 1;
}
