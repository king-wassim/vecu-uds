#include "vecu.h"

#include <string.h>
#include <time.h>

/* --- VIN, ECU serial, spare-part number — static demo identifiers. ---
 * Stored as plain ASCII so wireshark / candump output is human-readable.
 * 17-char VIN is the canonical SAE J853 / ISO 3779 size. */
static const uint8_t VIN_DATA[]    = "VF1234567890ABCDE";   /* 17 bytes */
static const uint8_t SERIAL_DATA[] = "ECU-DEMO-001";        /* 12 bytes */
static const uint8_t SPARE_DATA[]  = "SPN-0001";            /* 8 bytes  */

/*
 * Read-callback for the writable owner-name DID (0xF200).
 * The buffer lives in vecu_state_t and is protected by owner_lock.
 * We use a module-static back-reference for the same reason as app_engine.
 */
static vecu_state_t *g_state_for_owner = NULL;

static int owner_read(uint8_t *out, size_t cap, size_t *out_len) {
    if (g_state_for_owner == NULL || out == NULL || out_len == NULL) return -1;
    pthread_mutex_lock(&g_state_for_owner->owner_lock);
    size_t n = g_state_for_owner->owner_len;
    if (n > cap) n = cap;
    memcpy(out, g_state_for_owner->owner_name, n);
    *out_len = n;
    pthread_mutex_unlock(&g_state_for_owner->owner_lock);
    return 0;
}

static int owner_write(const uint8_t *in, size_t len) {
    if (g_state_for_owner == NULL || in == NULL) return -1;
    if (len == 0 || len > OWNER_NAME_CAP)        return -1;
    pthread_mutex_lock(&g_state_for_owner->owner_lock);
    memcpy(g_state_for_owner->owner_name, in, len);
    g_state_for_owner->owner_len = len;
    pthread_mutex_unlock(&g_state_for_owner->owner_lock);
    return 0;
}

void vecu_init(vecu_state_t *state) {
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));

    pthread_mutex_init(&state->session_lock, NULL);
    pthread_mutex_init(&state->owner_lock,   NULL);
    state->session = UDS_SESSION_DEFAULT;
    state->last_tester_present_ms = vecu_now_ms();

    /* Owner-name default. */
    const char *def = "ANONYMOUS";
    state->owner_len = strlen(def);
    memcpy(state->owner_name, def, state->owner_len);
    g_state_for_owner = state;

    /* Bring sub-modules up. */
    did_table_init(&state->dids);
    app_engine_init(&state->app);
    dtc_mem_init   (&state->dtcs);
    security_init  (&state->security);

    app_engine_install_did_callbacks(&state->app);

    /* ----- DID registration ----- */
    did_entry_t e;

    e = (did_entry_t){ .did = 0xF190, .static_data = VIN_DATA,    .static_len = 17 };
    did_table_add(&state->dids, &e);

    e = (did_entry_t){ .did = 0xF18C, .static_data = SERIAL_DATA, .static_len = 12 };
    did_table_add(&state->dids, &e);

    e = (did_entry_t){ .did = 0xF187, .static_data = SPARE_DATA,  .static_len = 8 };
    did_table_add(&state->dids, &e);

    e = (did_entry_t){ .did = 0x010C, .read = app_read_rpm };
    did_table_add(&state->dids, &e);

    e = (did_entry_t){ .did = 0x010D, .read = app_read_speed };
    did_table_add(&state->dids, &e);

    e = (did_entry_t){ .did = 0x0105, .read = app_read_coolant };
    did_table_add(&state->dids, &e);

    e = (did_entry_t){
        .did = 0xF200,
        .read = owner_read,
        .write = owner_write,
        .requires_security = true,
        .requires_extended_session = true,
    };
    did_table_add(&state->dids, &e);

    /* ----- Seed two DTCs so the diag tool sees something. ----- */
    /* P0301 = misfire cylinder 1 */
    dtc_mem_add(&state->dtcs, 0x030100u,
                DTC_STATUS_TEST_FAILED | DTC_STATUS_CONFIRMED |
                DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR);
    /* P0420 = catalyst efficiency below threshold (bank 1) */
    dtc_mem_add(&state->dtcs, 0x042000u,
                DTC_STATUS_TEST_FAILED | DTC_STATUS_CONFIRMED |
                DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR);
}

void vecu_destroy(vecu_state_t *state) {
    if (state == NULL) return;
    app_engine_destroy(&state->app);
    dtc_mem_destroy   (&state->dtcs);
    security_destroy  (&state->security);
    pthread_mutex_destroy(&state->session_lock);
    pthread_mutex_destroy(&state->owner_lock);
    g_state_for_owner = NULL;
}

uint8_t vecu_get_session(vecu_state_t *state) {
    pthread_mutex_lock(&state->session_lock);
    uint8_t s = state->session;
    pthread_mutex_unlock(&state->session_lock);
    return s;
}

void vecu_set_session(vecu_state_t *state, uint8_t session) {
    pthread_mutex_lock(&state->session_lock);
    state->session = session;
    state->last_tester_present_ms = vecu_now_ms();
    pthread_mutex_unlock(&state->session_lock);
}

void vecu_mark_tester_present(vecu_state_t *state) {
    pthread_mutex_lock(&state->session_lock);
    state->last_tester_present_ms = vecu_now_ms();
    pthread_mutex_unlock(&state->session_lock);
}

uint64_t vecu_last_tester_present_ms(vecu_state_t *state) {
    pthread_mutex_lock(&state->session_lock);
    uint64_t t = state->last_tester_present_ms;
    pthread_mutex_unlock(&state->session_lock);
    return t;
}

uint64_t vecu_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}
