#include "threads.h"
#include "uds.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ---------- rx_thread ----------
 *
 * Read complete UDS requests from the CAN_ISOTP socket and hand them off
 * to the dispatcher via the bounded queue. The socket is bound by main()
 * with a 1-second receive timeout (SO_RCVTIMEO) so a quiet bus does not
 * keep us blocked past shutdown.
 */
void *rx_thread_main(void *arg) {
    thread_ctx_t *ctx = (thread_ctx_t *)arg;
    uint8_t buf[UDS_MAX_MSG_LEN];
    size_t  len = 0;

    while (!*ctx->stop) {
        int rc = isotp_kio_recv(ctx->io, buf, sizeof(buf), &len);
        if (rc == 1)                 continue;  /* signal */
        if (rc != 0 && errno == EAGAIN) continue;  /* RCV timeout */
        if (rc != 0)                 break;     /* real error */
        if (len == 0)                continue;
        if (uds_queue_push(ctx->req_q, buf, len) != 0) break;
    }
    /* Wake the dispatcher so it sees shutdown. */
    uds_queue_shutdown(ctx->req_q);
    return NULL;
}

/* ---------- dispatch_thread ----------
 *
 * Pops one UDS request, runs the handler, sends back the response.
 * write() on the kernel isotp socket from this thread does not race
 * with read() from rx_thread.
 */
void *dispatch_thread_main(void *arg) {
    thread_ctx_t *ctx = (thread_ctx_t *)arg;
    uds_msg_t msg;
    uint8_t   resp[UDS_MAX_MSG_LEN];
    size_t    resp_len = 0;

    while (!*ctx->stop) {
        if (uds_queue_pop_blocking(ctx->req_q, &msg) != 0) break;

        int rc = uds_process_request(ctx->state,
                                     msg.data, msg.len,
                                     resp, sizeof(resp), &resp_len);
        if (rc < 0) continue;     /* malformed, drop */
        if (rc == 1) continue;    /* suppressed positive */
        if (resp_len == 0) continue;

        (void)isotp_kio_send(ctx->io, resp, resp_len);
    }
    return NULL;
}

/* ---------- app_thread ----------
 *
 * 10 Hz tick of the engine simulator. Sleep is interruptible via
 * the shutdown flag — checked between every nap.
 */
void *app_thread_main(void *arg) {
    thread_ctx_t *ctx = (thread_ctx_t *)arg;
    struct timespec interval = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };

    while (!*ctx->stop) {
        app_engine_tick(&ctx->state->app);
        nanosleep(&interval, NULL);
    }
    return NULL;
}

/* ---------- s3_thread ----------
 *
 * ISO 14229 S3 timer: in any session ≠ Default, if no TesterPresent is
 * received within S3_TIMEOUT_MS, revert to Default Session. This is what
 * keeps a real ECU from staying unlocked when the tester walks away.
 *
 * Polls every 500 ms — generous, since S3 is itself a coarse 5-second
 * timer. Polling is fine here, no cond var needed.
 */
void *s3_thread_main(void *arg) {
    thread_ctx_t *ctx = (thread_ctx_t *)arg;
    struct timespec interval = { .tv_sec = 0, .tv_nsec = 500 * 1000 * 1000 };

    while (!*ctx->stop) {
        uint8_t  sess = vecu_get_session(ctx->state);
        if (sess != UDS_SESSION_DEFAULT) {
            uint64_t now      = vecu_now_ms();
            uint64_t last_tp  = vecu_last_tester_present_ms(ctx->state);
            if (now - last_tp > S3_TIMEOUT_MS) {
                fprintf(stderr, "[S3] timeout — reverting to default session\n");
                vecu_set_session(ctx->state, UDS_SESSION_DEFAULT);
                /* Also lock security back. */
                pthread_mutex_lock(&ctx->state->security.lock);
                ctx->state->security.unlocked_level = 0;
                ctx->state->security.seed_issued    = false;
                pthread_mutex_unlock(&ctx->state->security.lock);
            }
        }
        nanosleep(&interval, NULL);
    }
    return NULL;
}
