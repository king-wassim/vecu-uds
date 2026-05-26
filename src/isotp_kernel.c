#include "isotp_kernel.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/isotp.h>

int isotp_kio_open(isotp_kio_t *k, const char *ifname,
                   uint32_t rx_id, uint32_t tx_id) {
    if (k == NULL || ifname == NULL) return -1;
    k->sock = -1;

    int s = socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP);
    if (s < 0) {
        fprintf(stderr, "isotp_kio: socket(CAN_ISOTP): %s\n", strerror(errno));
        fprintf(stderr, "          hint: sudo modprobe can-isotp\n");
        return -1;
    }

    /* Default option set: padding 0x00, pad rx/tx, BS=0 / STmin=0. */
    struct can_isotp_options opts;
    memset(&opts, 0, sizeof(opts));
    opts.flags    = CAN_ISOTP_TX_PADDING | CAN_ISOTP_RX_PADDING;
    opts.txpad_content = 0x00;
    opts.rxpad_content = 0x00;
    if (setsockopt(s, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts)) < 0) {
        fprintf(stderr, "isotp_kio: setsockopt(OPTS): %s\n", strerror(errno));
        (void)close(s);
        return -1;
    }

    struct can_isotp_fc_options fc;
    memset(&fc, 0, sizeof(fc));
    fc.bs    = 0;
    fc.stmin = 0;
    fc.wftmax = 0;
    (void)setsockopt(s, SOL_CAN_ISOTP, CAN_ISOTP_RECV_FC, &fc, sizeof(fc));

    unsigned ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        fprintf(stderr, "isotp_kio: if_nametoindex(%s): %s\n", ifname, strerror(errno));
        (void)close(s);
        return -1;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = (int)ifindex;
    addr.can_addr.tp.rx_id = rx_id;
    addr.can_addr.tp.tx_id = tx_id;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "isotp_kio: bind(rx=0x%X tx=0x%X): %s\n",
                rx_id, tx_id, strerror(errno));
        (void)close(s);
        return -1;
    }

    k->sock = s;
    return 0;
}

int isotp_kio_recv(isotp_kio_t *k, uint8_t *out, size_t cap, size_t *out_len) {
    if (k == NULL || out == NULL || out_len == NULL || k->sock < 0) return -1;
    ssize_t n = read(k->sock, out, cap);
    if (n < 0) {
        if (errno == EINTR) return 1;
        /* EAGAIN/EWOULDBLOCK = SO_RCVTIMEO expired with no data — normal,
         * the caller (rx_thread) will retry. Don't spam the log. */
        if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;
        fprintf(stderr, "isotp_kio: read: %s\n", strerror(errno));
        return -1;
    }
    *out_len = (size_t)n;
    return 0;
}

int isotp_kio_send(isotp_kio_t *k, const uint8_t *data, size_t len) {
    if (k == NULL || data == NULL || k->sock < 0) return -1;
    ssize_t n = write(k->sock, data, len);
    if (n != (ssize_t)len) {
        fprintf(stderr, "isotp_kio: write: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void isotp_kio_close(isotp_kio_t *k) {
    if (k == NULL) return;
    if (k->sock >= 0) (void)close(k->sock);
    k->sock = -1;
}
