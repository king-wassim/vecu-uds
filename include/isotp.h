#ifndef VECU_ISOTP_H
#define VECU_ISOTP_H

/*
 * ISO 15765-2 (ISO-TP) transport layer — minimal but spec-compliant subset.
 *
 * Supports:
 *   - Single Frame  (SF, payload 1..7 bytes)
 *   - First Frame   (FF, payload up to 4095 bytes)
 *   - Consecutive Frame (CF, sequence-numbered)
 *   - Flow Control  (FC, BS=0 / STmin=0 — "send the whole thing, no wait")
 *
 * Out of scope (deliberately, since we own both ends in the demo):
 *   - Extended addressing
 *   - WAIT / OVERFLOW FC responses
 *   - N_As / N_Bs timeouts (only N_Cr on the receive side)
 */

#include <stddef.h>
#include <stdint.h>

#include "can_io.h"

#define ISOTP_MAX_PAYLOAD 4095   /* ISO 15765-2 §6.5.3.3 (FF_DL is 12 bits) */
#define ISOTP_PADDING     0x00
#define ISOTP_N_CR_MS     1000   /* timeout between consecutive frames */

/* Tester ↔ ECU CAN IDs — OBD-II / UDS convention, configurable. */
typedef struct {
    uint32_t rx_id;   /* CAN ID we receive requests on  (tester→ECU) */
    uint32_t tx_id;   /* CAN ID we send responses on    (ECU→tester) */
} isotp_addr_t;

/*
 * Receive ONE complete ISO-TP message from the bus.
 *
 * Blocks on can_io_recv until either:
 *   - a complete message is reassembled  → returns 0, *out_len set
 *   - a non-recoverable error occurs     → returns -1
 *   - the operation is interrupted       → returns 1 (caller should check stop flag)
 *
 * Frames not addressed to `addr.rx_id` are silently skipped.
 * Out-of-state CFs (CF without preceding FF) are ignored per ISO 15765-2 §6.7.
 */
int isotp_recv(can_io_t *io,
               const isotp_addr_t *addr,
               uint8_t *out, size_t out_cap, size_t *out_len);

/*
 * Send ONE complete ISO-TP message on the bus.
 *
 * Automatically chooses SF or FF+CF based on length. For multi-frame,
 * waits for the FC from the peer (with a 1s timeout) before sending CFs.
 *
 * Returns 0 on success, -1 on error.
 */
int isotp_send(can_io_t *io,
               const isotp_addr_t *addr,
               const uint8_t *data, size_t len);

#endif /* VECU_ISOTP_H */
