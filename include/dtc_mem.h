#ifndef VECU_DTC_MEM_H
#define VECU_DTC_MEM_H

/*
 * DTC (Diagnostic Trouble Code) memory — small in-RAM store of active fault
 * codes for ReadDTCInformation (0x19 sub 0x02) and ClearDiagnosticInformation
 * (0x14).
 *
 * DTC encoding follows SAE J2012 / ISO 14229-1:
 *   3 bytes for the code itself (e.g. P0301 → 0x03 0x01 0x00 — first nibble
 *   00xx = P powertrain, 01xx = C, 10xx = B, 11xx = U)
 *   1 byte status bits (testFailed, confirmedDTC, ...)
 *
 * For the demo we seed 2-3 codes at boot. A real ECU would set them based
 * on physical fault conditions detected by application monitors.
 */

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DTC_MAX 16

/* Status bit flags — ISO 14229-1 Annex D */
#define DTC_STATUS_TEST_FAILED                     0x01
#define DTC_STATUS_TEST_FAILED_THIS_CYCLE          0x02
#define DTC_STATUS_PENDING                         0x04
#define DTC_STATUS_CONFIRMED                       0x08
#define DTC_STATUS_TEST_NOT_COMPLETED_SINCE_CLEAR  0x10
#define DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR    0x20
#define DTC_STATUS_TEST_NOT_COMPLETED_THIS_CYCLE   0x40
#define DTC_STATUS_WARNING_INDICATOR_REQUESTED     0x80

typedef struct {
    uint32_t dtc;     /* 24-bit DTC packed in low 3 bytes */
    uint8_t  status;  /* DTC status byte */
} dtc_entry_t;

typedef struct {
    pthread_mutex_t lock;
    dtc_entry_t     entries[DTC_MAX];
    size_t          count;
    /* Availability mask reported with the response.
     * Bit set = the ECU implements monitoring for that status condition. */
    uint8_t         availability_mask;
} dtc_mem_t;

void dtc_mem_init(dtc_mem_t *m);
void dtc_mem_destroy(dtc_mem_t *m);

/* Add a DTC entry. Returns 0 on success, -1 if full. */
int  dtc_mem_add(dtc_mem_t *m, uint32_t dtc, uint8_t status);

/* Clear all entries. Returns the number erased. */
size_t dtc_mem_clear_all(dtc_mem_t *m);

/* Snapshot under lock — copies up to max entries matching status_mask.
 * Returns number of matches written to `out`. */
size_t dtc_mem_snapshot_by_mask(dtc_mem_t *m, uint8_t status_mask,
                                dtc_entry_t *out, size_t max);

uint8_t dtc_mem_availability_mask(const dtc_mem_t *m);

#endif /* VECU_DTC_MEM_H */
