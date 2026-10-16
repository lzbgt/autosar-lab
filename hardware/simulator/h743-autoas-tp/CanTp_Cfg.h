/* Single-channel AutoAS CAN Transport Layer configuration for Renode.
 * Real AutoAS CanTp.c is used unchanged; upper-layer routing is a bounded
 * application test adapter, not the full Protocol Data Unit Router (PduR).
 */
#ifndef CANTP_CFG_H
#define CANTP_CFG_H
#define CANTP_USE_TX_CONFIRMATION
#define CANTP_MAIN_FUNCTION_PERIOD 10u
#define CANTP_STMIN_ADJUST 0u
#define CANTP_CONVERT_MS_TO_MAIN_CYCLES(ms) \
    (((ms) + CANTP_MAIN_FUNCTION_PERIOD - 1u) / CANTP_MAIN_FUNCTION_PERIOD)
#endif
