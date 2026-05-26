#include "dtc_mem.h"

#include <string.h>

void dtc_mem_init(dtc_mem_t *m) {
    if (m == NULL) return;
    memset(m, 0, sizeof(*m));
    pthread_mutex_init(&m->lock, NULL);
    /* Tell the tester which status bits we actually monitor.
     * For the demo we report testFailed + confirmedDTC capability. */
    m->availability_mask =
        DTC_STATUS_TEST_FAILED | DTC_STATUS_CONFIRMED |
        DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR;
}

void dtc_mem_destroy(dtc_mem_t *m) {
    if (m == NULL) return;
    pthread_mutex_destroy(&m->lock);
}

int dtc_mem_add(dtc_mem_t *m, uint32_t dtc, uint8_t status) {
    if (m == NULL) return -1;
    int rc = -1;
    pthread_mutex_lock(&m->lock);
    if (m->count < DTC_MAX) {
        m->entries[m->count].dtc    = dtc & 0x00FFFFFFu;
        m->entries[m->count].status = status;
        m->count++;
        rc = 0;
    }
    pthread_mutex_unlock(&m->lock);
    return rc;
}

size_t dtc_mem_clear_all(dtc_mem_t *m) {
    if (m == NULL) return 0;
    pthread_mutex_lock(&m->lock);
    size_t n = m->count;
    m->count = 0;
    memset(m->entries, 0, sizeof(m->entries));
    pthread_mutex_unlock(&m->lock);
    return n;
}

size_t dtc_mem_snapshot_by_mask(dtc_mem_t *m, uint8_t status_mask,
                                dtc_entry_t *out, size_t max) {
    if (m == NULL || out == NULL || max == 0) return 0;
    size_t written = 0;
    pthread_mutex_lock(&m->lock);
    for (size_t i = 0; i < m->count && written < max; ++i) {
        if ((m->entries[i].status & status_mask) != 0) {
            out[written++] = m->entries[i];
        }
    }
    pthread_mutex_unlock(&m->lock);
    return written;
}

uint8_t dtc_mem_availability_mask(const dtc_mem_t *m) {
    if (m == NULL) return 0;
    /* Const-correct read of a single byte — atomic on every platform we
     * target. No need to take the lock for this. */
    return m->availability_mask;
}
