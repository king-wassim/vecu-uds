#include "isotp.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <linux/can.h>

/*
 * PCI type lives in the high nibble of byte 0:
 *   0x0_ = SF, 0x1_ = FF, 0x2_ = CF, 0x3_ = FC.
 * The low nibble carries length (SF), high 4 bits of length (FF),
 * sequence number (CF) or flow status (FC).
 */
#define PCI_TYPE(b)  ((uint8_t)((b) >> 4))
#define PCI_SF 0x0u
#define PCI_FF 0x1u
#define PCI_CF 0x2u
#define PCI_FC 0x3u

#define FC_FS_CTS      0x0u
#define FC_FS_WAIT     0x1u
#define FC_FS_OVERFLOW 0x2u

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

static void frame_pad(struct can_frame *f, size_t used) {
    for (size_t i = used; i < 8; ++i) {
        f->data[i] = ISOTP_PADDING;
    }
    f->can_dlc = 8;  /* always pad to 8 — improves analyzer compatibility */
}

/* ---------- TX ---------- */

static int send_sf(can_io_t *io, uint32_t tx_id,
                   const uint8_t *data, size_t len) {
    /* SF format: byte0 = 0x0L (L=len 1..7), bytes1..L = payload. */
    struct can_frame f;
    memset(&f, 0, sizeof(f));
    f.can_id  = tx_id & CAN_SFF_MASK;
    f.data[0] = (uint8_t)(len & 0x0F);
    memcpy(&f.data[1], data, len);
    frame_pad(&f, 1 + len);
    return can_io_send(io, &f);
}

/* Wait (blocking) for a Flow Control frame from peer (tester).
 * Returns 0 if CTS received, -1 on error or non-CTS/timeout. */
static int wait_for_fc(can_io_t *io, uint32_t expected_id) {
    uint64_t deadline = now_ms() + ISOTP_N_CR_MS;

    for (;;) {
        struct can_frame f;
        int rc = can_io_recv(io, &f);
        if (rc == 1) continue;          /* EINTR — retry */
        if (rc != 0) return -1;

        uint32_t id = f.can_id & CAN_SFF_MASK;
        if (id != expected_id) {
            /* Not our flow control — drop it. */
            if (now_ms() > deadline) {
                fprintf(stderr, "isotp: timeout waiting for FC\n");
                return -1;
            }
            continue;
        }

        if (PCI_TYPE(f.data[0]) != PCI_FC) {
            fprintf(stderr, "isotp: expected FC, got 0x%02X\n", f.data[0]);
            return -1;
        }

        uint8_t fs = f.data[0] & 0x0F;
        if (fs == FC_FS_CTS)      return 0;
        if (fs == FC_FS_WAIT)     { /* would loop, but BS=0 from tester is fine */ continue; }
        if (fs == FC_FS_OVERFLOW) { fprintf(stderr, "isotp: FC OVERFLOW from peer\n"); return -1; }
        return -1;
    }
}

static int send_multiframe(can_io_t *io, const isotp_addr_t *addr,
                           const uint8_t *data, size_t len) {
    /* FF: byte0 = 0x1X (X = high 4 bits of len), byte1 = low 8 bits of len.
     * bytes 2..7 = first 6 payload bytes. */
    struct can_frame f;
    memset(&f, 0, sizeof(f));
    f.can_id  = addr->tx_id & CAN_SFF_MASK;
    f.data[0] = (uint8_t)(0x10 | ((len >> 8) & 0x0F));
    f.data[1] = (uint8_t)(len & 0xFF);
    memcpy(&f.data[2], data, 6);
    f.can_dlc = 8;
    if (can_io_send(io, &f) != 0) return -1;

    /* Wait for FC from tester. */
    if (wait_for_fc(io, addr->rx_id) != 0) return -1;

    /* CFs: byte0 = 0x2N (N = sequence number 1..15 wrapping). */
    size_t offset = 6;
    uint8_t sn = 1;
    while (offset < len) {
        memset(&f, 0, sizeof(f));
        f.can_id  = addr->tx_id & CAN_SFF_MASK;
        f.data[0] = (uint8_t)(0x20 | (sn & 0x0F));
        size_t chunk = (len - offset > 7) ? 7 : (len - offset);
        memcpy(&f.data[1], data + offset, chunk);
        frame_pad(&f, 1 + chunk);
        if (can_io_send(io, &f) != 0) return -1;
        offset += chunk;
        sn = (uint8_t)((sn + 1) & 0x0F);
    }
    return 0;
}

int isotp_send(can_io_t *io, const isotp_addr_t *addr,
               const uint8_t *data, size_t len) {
    if (io == NULL || addr == NULL || data == NULL) return -1;
    if (len == 0 || len > ISOTP_MAX_PAYLOAD)       return -1;

    if (len <= 7) {
        return send_sf(io, addr->tx_id, data, len);
    }
    return send_multiframe(io, addr, data, len);
}

/* ---------- RX ---------- */

static int send_fc_cts(can_io_t *io, uint32_t tx_id) {
    struct can_frame f;
    memset(&f, 0, sizeof(f));
    f.can_id  = tx_id & CAN_SFF_MASK;
    f.data[0] = (uint8_t)(0x30 | FC_FS_CTS);  /* PCI=FC, FS=CTS */
    f.data[1] = 0x00;                          /* BS=0 → send all */
    f.data[2] = 0x00;                          /* STmin=0 → no delay */
    frame_pad(&f, 3);
    return can_io_send(io, &f);
}

int isotp_recv(can_io_t *io, const isotp_addr_t *addr,
               uint8_t *out, size_t out_cap, size_t *out_len) {
    if (io == NULL || addr == NULL || out == NULL || out_len == NULL) return -1;

    bool   receiving = false;
    size_t total_len = 0;
    size_t got       = 0;
    uint8_t next_sn  = 1;
    uint64_t deadline = 0;

    for (;;) {
        struct can_frame f;
        int rc = can_io_recv(io, &f);
        if (rc == 1) return 1;     /* interrupted, let caller decide */
        if (rc != 0) return -1;

        uint32_t id = f.can_id & CAN_SFF_MASK;
        if (id != addr->rx_id) {
            continue;  /* not for us */
        }

        uint8_t pci = PCI_TYPE(f.data[0]);

        if (!receiving) {
            if (pci == PCI_SF) {
                size_t len = f.data[0] & 0x0F;
                if (len == 0 || len > 7 || len > out_cap) {
                    fprintf(stderr, "isotp: malformed SF (len=%zu)\n", len);
                    continue;
                }
                memcpy(out, &f.data[1], len);
                *out_len = len;
                return 0;
            }
            if (pci == PCI_FF) {
                total_len = (size_t)(((f.data[0] & 0x0F) << 8) | f.data[1]);
                if (total_len < 8 || total_len > ISOTP_MAX_PAYLOAD || total_len > out_cap) {
                    fprintf(stderr, "isotp: FF with bad len %zu\n", total_len);
                    continue;
                }
                memcpy(out, &f.data[2], 6);
                got = 6;
                next_sn = 1;
                receiving = true;
                if (send_fc_cts(io, addr->tx_id) != 0) return -1;
                deadline = now_ms() + ISOTP_N_CR_MS;
                continue;
            }
            /* CF or FC out of state → ignore per spec */
            continue;
        }

        /* receiving == true */
        if (now_ms() > deadline) {
            fprintf(stderr, "isotp: N_Cr timeout, abort\n");
            return -1;
        }

        if (pci != PCI_CF) {
            /* New FF / SF while reassembling → ISO-TP §6.5.5: abort current
             * and start fresh. Keep it simple: just drop the current assembly. */
            fprintf(stderr, "isotp: unexpected PCI 0x%X during reassembly\n", pci);
            receiving = false;
            continue;
        }

        uint8_t sn = f.data[0] & 0x0F;
        if (sn != next_sn) {
            fprintf(stderr, "isotp: SN mismatch (got %u, expected %u)\n", sn, next_sn);
            return -1;
        }
        size_t remaining = total_len - got;
        size_t chunk = (remaining > 7) ? 7 : remaining;
        memcpy(out + got, &f.data[1], chunk);
        got += chunk;
        next_sn = (uint8_t)((next_sn + 1) & 0x0F);
        deadline = now_ms() + ISOTP_N_CR_MS;

        if (got >= total_len) {
            *out_len = total_len;
            return 0;
        }
    }
}
