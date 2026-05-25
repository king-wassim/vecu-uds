#ifndef VECU_CAN_IO_H
#define VECU_CAN_IO_H

/*
 * can_io — thin wrapper over Linux SocketCAN (PF_CAN / CAN_RAW).
 *
 * Layer responsibility: open/close a CAN_RAW socket bound to a given
 * interface (vcan0, can0, ...) and ship raw struct can_frame back and
 * forth. No filtering, no protocol logic — that lives one level up.
 */

#include <stddef.h>
#include <stdint.h>
#include <linux/can.h>

#define CAN_IO_IFNAME_MAX 16  /* matches IFNAMSIZ */

typedef struct {
    int  sock;                       /* file descriptor, -1 if not open */
    char ifname[CAN_IO_IFNAME_MAX];  /* zero-terminated copy of the iface */
} can_io_t;

/* Open a CAN_RAW socket and bind it to `ifname`.
 * Returns 0 on success, -1 on error (errno preserved on syscall failure). */
int can_io_open(can_io_t *io, const char *ifname);

/* Blocking read of one CAN frame.
 * Returns  0 on success,
 *         +1 if the read was interrupted by a signal (caller should retry),
 *         -1 on any other error. */
int can_io_recv(can_io_t *io, struct can_frame *frame);

/* Blocking write of one CAN frame.
 * Returns 0 on success, -1 on error. */
int can_io_send(can_io_t *io, const struct can_frame *frame);

/* Close the socket (no-op if already closed). */
void can_io_close(can_io_t *io);

#endif /* VECU_CAN_IO_H */
