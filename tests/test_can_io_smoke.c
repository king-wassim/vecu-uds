#include "can_io.h"

#include <assert.h>
#include <stdio.h>

/*
 * can_io smoke test — runnable in any Linux environment, including CI
 * without vcan: we only check NULL safety and that opening a bogus
 * interface fails gracefully.
 *
 * A real loopback test against vcan0 will come once we wire CI up with
 * a kernel that has the vcan module available.
 */
int main(void) {
    can_io_t io;

    /* NULL safety. */
    assert(can_io_open(NULL, "vcan0") != 0);
    assert(can_io_open(&io, NULL) != 0);

    /* Non-existent interface should fail cleanly, not crash. */
    int rc = can_io_open(&io, "vecu_uds_no_such_iface");
    assert(rc != 0);

    /* close() on a never-opened struct must be a no-op. */
    can_io_t fresh = { .sock = -1, .ifname = {0} };
    can_io_close(&fresh);
    can_io_close(NULL);

    puts("OK");
    return 0;
}
