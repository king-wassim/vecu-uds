#include "queue.h"

#include <string.h>

void uds_queue_init(uds_queue_t *q) {
    if (q == NULL) return;
    memset(q, 0, sizeof(*q));
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init (&q->not_empty, NULL);
    pthread_cond_init (&q->not_full,  NULL);
}

void uds_queue_destroy(uds_queue_t *q) {
    if (q == NULL) return;
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    pthread_mutex_destroy(&q->lock);
}

void uds_queue_shutdown(uds_queue_t *q) {
    if (q == NULL) return;
    pthread_mutex_lock(&q->lock);
    q->closed = true;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
}

int uds_queue_push(uds_queue_t *q, const uint8_t *data, size_t len) {
    if (q == NULL || data == NULL || len == 0 || len > UDS_MAX_MSG_LEN) return -1;
    pthread_mutex_lock(&q->lock);
    while (!q->closed && q->count == UDS_QUEUE_CAP) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    if (q->closed) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    uds_msg_t *slot = &q->slots[q->tail];
    memcpy(slot->data, data, len);
    slot->len = len;
    q->tail = (q->tail + 1) % UDS_QUEUE_CAP;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int uds_queue_pop_blocking(uds_queue_t *q, uds_msg_t *out) {
    if (q == NULL || out == NULL) return -1;
    pthread_mutex_lock(&q->lock);
    while (!q->closed && q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->count == 0) {
        /* closed and drained */
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    *out = q->slots[q->head];
    q->head = (q->head + 1) % UDS_QUEUE_CAP;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0;
}
