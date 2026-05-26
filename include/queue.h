#ifndef VECU_QUEUE_H
#define VECU_QUEUE_H

/*
 * Bounded blocking FIFO of UDS messages, producer/consumer pattern.
 *
 * Producers (rx_thread) call uds_queue_push;
 * Consumers (dispatch_thread) call uds_queue_pop_blocking.
 *
 * On shutdown, queue_shutdown() wakes all waiters and marks the queue
 * closed; subsequent push/pop return -1.
 */

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uds_types.h"

#define UDS_QUEUE_CAP 8

typedef struct {
    uint8_t data[UDS_MAX_MSG_LEN];
    size_t  len;
} uds_msg_t;

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    uds_msg_t       slots[UDS_QUEUE_CAP];
    size_t          head;       /* pop index */
    size_t          tail;       /* push index */
    size_t          count;
    bool            closed;
} uds_queue_t;

void uds_queue_init    (uds_queue_t *q);
void uds_queue_destroy (uds_queue_t *q);
void uds_queue_shutdown(uds_queue_t *q);

/* Blocking push (waits if full). Returns 0 / -1 if closed. */
int  uds_queue_push(uds_queue_t *q, const uint8_t *data, size_t len);

/* Blocking pop. Returns 0 with *msg populated, or -1 if closed and empty. */
int  uds_queue_pop_blocking(uds_queue_t *q, uds_msg_t *out);

#endif /* VECU_QUEUE_H */
