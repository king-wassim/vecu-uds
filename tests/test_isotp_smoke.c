#include "isotp.h"
#include "can_io.h"

#include <assert.h>
#include <stdio.h>

/*
 * ISO-TP smoke test — NULL safety + bad-arg guards.
 * Real protocol behavior is validated end-to-end by the Python pytest
 * suite (client/tests/test_isotp.py) which uses a real vcan0.
 */
int main(void) {
    can_io_t io = { .sock = -1, .ifname = {0} };
    isotp_addr_t addr = { .rx_id = 0x7E0, .tx_id = 0x7E8 };
    uint8_t buf[32];
    size_t  blen = 0;
    uint8_t one  = 0x55;

    /* NULL guards on send. */
    assert(isotp_send(NULL, &addr, &one, 1) != 0);
    assert(isotp_send(&io,  NULL, &one, 1) != 0);
    assert(isotp_send(&io, &addr, NULL, 1) != 0);

    /* Length guards on send. */
    assert(isotp_send(&io, &addr, &one, 0) != 0);
    assert(isotp_send(&io, &addr, &one, ISOTP_MAX_PAYLOAD + 1) != 0);

    /* NULL guards on recv. */
    assert(isotp_recv(NULL, &addr, buf, sizeof(buf), &blen) != 0);
    assert(isotp_recv(&io,  NULL, buf, sizeof(buf), &blen) != 0);
    assert(isotp_recv(&io, &addr, NULL, sizeof(buf), &blen) != 0);
    assert(isotp_recv(&io, &addr, buf, sizeof(buf), NULL)   != 0);

    puts("OK");
    return 0;
}
