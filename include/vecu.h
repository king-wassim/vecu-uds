#ifndef VECU_H
#define VECU_H

/*
 * vecu_state_t — central ECU context.
 *
 * Composes all sub-modules (DID table, app telemetry, DTC memory,
 * security state) plus session state and the writable owner-name DID
 * buffer. A single instance lives in main() and is passed by pointer to
 * the UDS handlers and the worker threads.
 *
 * Thread-safety: each sub-module owns its own mutex (see its header).
 * `session_lock` protects the session/S3-timer fields below. Lock order
 * if multiple are held simultaneously:
 *     session_lock < did mutex < dtc_mem.lock < security.lock
 * (currently no handler takes two of these at once, but document early).
 */

#include <pthread.h>
#include <stdbool.h>

#include "uds_types.h"
#include "did_table.h"
#include "app_engine.h"
#include "dtc_mem.h"
#include "security.h"

/* Writable owner name (DID 0xF200) — capped buffer + actual length. */
#define OWNER_NAME_CAP 32

typedef struct {
    pthread_mutex_t session_lock;
    uint8_t  session;                    /* uds_session_t */
    uint64_t last_tester_present_ms;     /* monotonic, for S3 timer */

    did_table_t      dids;
    app_engine_t     app;
    dtc_mem_t        dtcs;
    security_state_t security;

    pthread_mutex_t owner_lock;
    uint8_t  owner_name[OWNER_NAME_CAP];
    size_t   owner_len;
} vecu_state_t;

void vecu_init(vecu_state_t *state);
void vecu_destroy(vecu_state_t *state);

/* Read/write helpers for session (always taken under lock). */
uint8_t  vecu_get_session(vecu_state_t *state);
void     vecu_set_session(vecu_state_t *state, uint8_t session);
void     vecu_mark_tester_present(vecu_state_t *state);
uint64_t vecu_last_tester_present_ms(vecu_state_t *state);

/* Monotonic clock helper used across modules. */
uint64_t vecu_now_ms(void);

#endif /* VECU_H */
