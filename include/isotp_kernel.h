#ifndef VECU_ISOTP_KERNEL_H
#define VECU_ISOTP_KERNEL_H

/*
 * Thin wrapper over the Linux kernel can-isotp module.
 *
 * Why this exists alongside our custom isotp.c:
 *   - isotp.c is the "I built it to understand it" version. It validates
 *     the protocol knowledge for interview discussions.
 *   - isotp_kernel.c is the "ship it" version. Read/write deliver
 *     complete UDS messages, the kernel handles SF/FF/CF/FC, timing,
 *     reassembly, padding. Indispensable for the multi-thread runtime
 *     because read() and write() on the same fd from two threads are
 *     safe (no userspace state machine to corrupt).
 *
 * Requires the `can-isotp` kernel module to be loaded:
 *     sudo modprobe can-isotp
 */

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int sock;
} isotp_kio_t;

/* Open a CAN_ISOTP socket bound to (rx_id, tx_id) on `ifname`.
 * Returns 0 on success. Sets errno on failure. */
int  isotp_kio_open(isotp_kio_t *k, const char *ifname,
                    uint32_t rx_id, uint32_t tx_id);

/* Receive ONE complete UDS message. Blocks. Returns
 *   0  success, *out_len set
 *   1  interrupted by signal (caller may retry)
 *  -1  error */
int  isotp_kio_recv(isotp_kio_t *k, uint8_t *out, size_t cap, size_t *out_len);

/* Send ONE complete UDS message. Returns 0 / -1. */
int  isotp_kio_send(isotp_kio_t *k, const uint8_t *data, size_t len);

void isotp_kio_close(isotp_kio_t *k);

#endif /* VECU_ISOTP_KERNEL_H */
