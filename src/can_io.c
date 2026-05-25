#include "can_io.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int can_io_open(can_io_t *io, const char *ifname) {
    if (io == NULL || ifname == NULL) {
        return -1;
    }

    memset(io, 0, sizeof(*io));
    io->sock = -1;

    /* Copy interface name (always zero-terminated thanks to memset above). */
    strncpy(io->ifname, ifname, sizeof(io->ifname) - 1);

    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        fprintf(stderr, "can_io: socket(PF_CAN, SOCK_RAW, CAN_RAW): %s\n",
                strerror(errno));
        return -1;
    }

    unsigned int ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        fprintf(stderr, "can_io: if_nametoindex(%s): %s\n",
                ifname, strerror(errno));
        (void)close(s);
        return -1;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = (int)ifindex;

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "can_io: bind(%s): %s\n", ifname, strerror(errno));
        (void)close(s);
        return -1;
    }

    io->sock = s;
    return 0;
}

int can_io_recv(can_io_t *io, struct can_frame *frame) {
    if (io == NULL || frame == NULL || io->sock < 0) {
        return -1;
    }
    ssize_t n = read(io->sock, frame, sizeof(*frame));
    if (n < 0) {
        if (errno == EINTR) {
            return 1;  /* interrupted by signal, caller may retry */
        }
        fprintf(stderr, "can_io: read: %s\n", strerror(errno));
        return -1;
    }
    if (n != (ssize_t)sizeof(*frame)) {
        fprintf(stderr, "can_io: short read (%zd bytes, expected %zu)\n",
                n, sizeof(*frame));
        return -1;
    }
    return 0;
}

int can_io_send(can_io_t *io, const struct can_frame *frame) {
    if (io == NULL || frame == NULL || io->sock < 0) {
        return -1;
    }
    ssize_t n = write(io->sock, frame, sizeof(*frame));
    if (n != (ssize_t)sizeof(*frame)) {
        fprintf(stderr, "can_io: write: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void can_io_close(can_io_t *io) {
    if (io == NULL) {
        return;
    }
    if (io->sock >= 0) {
        (void)close(io->sock);
    }
    io->sock = -1;
}
