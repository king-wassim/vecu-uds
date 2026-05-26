/*
 * vecu-uds — virtual ECU runtime.
 *
 * Spawns four worker threads on top of a kernel CAN_ISOTP socket:
 *
 *     +-----------+   uds_msg   +-----------------+
 *     | rx_thread |  --queue--> | dispatch_thread |  --write()--> bus
 *     +-----------+             +-----------------+
 *                                       ^
 *                                       | UDS handlers read/write
 *                  +------------+       |       +------------+
 *                  | app_thread | ---updates--> |  vECU      |
 *                  +------------+   shared      |  state     |
 *                                   structures  +------------+
 *                  +------------+       ^
 *                  | s3_thread  | ------+
 *                  +------------+   resets session on TP timeout
 *
 * Why kernel can-isotp here (and not our custom isotp.c) :
 * read() and write() on the same fd from two different threads are
 * safe — the kernel serialises them. Doing the same with our custom
 * isotp.c (which mixes RX and TX inside a single send call to wait for
 * the FC) would require us to ferry FC frames between threads ourselves.
 * The kernel module already does that. Our isotp.c stays as a tested
 * standalone library that proves we understand the protocol.
 */

#include "vecu.h"
#include "uds.h"
#include "isotp_kernel.h"
#include "queue.h"
#include "threads.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

int main(int argc, char **argv) {
    const char *ifname = (argc > 1) ? argv[1] : "vcan0";
    uint32_t rx_id     = 0x7E0;   /* tester → ECU */
    uint32_t tx_id     = 0x7E8;   /* ECU → tester */

    printf("vecu-uds v1.0.0 — starting on %s (rx=0x%03X tx=0x%03X)\n",
           ifname, rx_id, tx_id);
    fflush(stdout);

    /* ----- ECU state ----- */
    vecu_state_t state;
    vecu_init(&state);

    /* ----- ISO-TP kernel socket ----- */
    isotp_kio_t io;
    if (isotp_kio_open(&io, ifname, rx_id, tx_id) != 0) {
        fprintf(stderr,
                "Failed to open CAN_ISOTP socket on '%s'.\n"
                "Hint: sudo modprobe can-isotp && ./scripts/setup_vcan.sh %s\n",
                ifname, ifname);
        vecu_destroy(&state);
        return EXIT_FAILURE;
    }

    /* 1-second read timeout so rx_thread can wake up to check g_stop. */
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    (void)setsockopt(io.sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* ----- Request queue ----- */
    uds_queue_t req_q;
    uds_queue_init(&req_q);

    /* ----- Signal handlers ----- */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* ----- Spawn threads ----- */
    thread_ctx_t ctx = {
        .state = &state,
        .io    = &io,
        .req_q = &req_q,
        .stop  = &g_stop,
    };
    pthread_t t_rx, t_disp, t_app, t_s3;
    pthread_create(&t_rx,   NULL, rx_thread_main,       &ctx);
    pthread_create(&t_disp, NULL, dispatch_thread_main, &ctx);
    pthread_create(&t_app,  NULL, app_thread_main,      &ctx);
    pthread_create(&t_s3,   NULL, s3_thread_main,       &ctx);

    printf("vecu-uds ready. Ctrl+C to stop.\n");
    fflush(stdout);

    /* ----- Wait for signal ----- */
    while (!g_stop) {
        pause();   /* sleep until any signal */
    }

    fprintf(stderr, "\n[main] shutting down...\n");
    uds_queue_shutdown(&req_q);
    isotp_kio_close(&io);   /* unblocks the read() in rx_thread with EBADF */

    pthread_join(t_rx,   NULL);
    pthread_join(t_disp, NULL);
    pthread_join(t_app,  NULL);
    pthread_join(t_s3,   NULL);

    uds_queue_destroy(&req_q);
    vecu_destroy(&state);
    printf("Bye.\n");
    return EXIT_SUCCESS;
}
