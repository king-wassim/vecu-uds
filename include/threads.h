#ifndef VECU_THREADS_H
#define VECU_THREADS_H

/*
 * Worker thread entry points.
 *
 * Layout:
 *   - rx_thread       : reads complete UDS requests from the CAN_ISOTP
 *                       socket, pushes them onto the request queue.
 *   - dispatch_thread : pops requests, calls uds_process_request,
 *                       writes responses back to the socket.
 *   - app_thread      : ticks the app_engine at ~10 Hz.
 *   - s3_thread       : every ~500 ms, checks the tester-present
 *                       timestamp and reverts to default session
 *                       after 5 s of silence in non-default sessions.
 *
 * Shutdown: main() sets `g_stop` (sig_atomic_t) and shuts the queue.
 * All threads check the flag at their wait points and exit cleanly.
 */

#include <pthread.h>
#include <signal.h>

#include "vecu.h"
#include "queue.h"
#include "isotp_kernel.h"

#define S3_TIMEOUT_MS 5000

typedef struct {
    vecu_state_t      *state;
    isotp_kio_t       *io;
    uds_queue_t       *req_q;
    volatile sig_atomic_t *stop;
} thread_ctx_t;

void *rx_thread_main      (void *arg);
void *dispatch_thread_main(void *arg);
void *app_thread_main     (void *arg);
void *s3_thread_main      (void *arg);

#endif /* VECU_THREADS_H */
