#include "uds.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*
 * UDS service dispatcher.
 *
 * Each implemented service is a small handler that consumes the bytes
 * after the SID and writes a positive or negative response. The
 * outer uds_process_request finds the right handler by linear scan over
 * a SID→handler table. With <10 services this is fine; a real ECU with
 * 30+ services would use a sorted table + bsearch.
 *
 * Handler return convention:
 *    0  → response written to resp[0..*resp_len)
 *    1  → suppressed positive response (caller must not transmit)
 *   -1  → unrecoverable (caller drops the request)
 *
 * Sub-function services interpret bit 7 of the sub-function byte as
 * "suppress positive response" (ISO 14229-1 §7.5.4). We mask it before
 * comparison and only suppress the positive case — NRCs are always sent.
 */

typedef int (*uds_handler_fn)(vecu_state_t *state,
                              const uint8_t *req, size_t req_len,
                              uint8_t *resp, size_t resp_cap, size_t *resp_len);

/* ---------- helpers ---------- */

static size_t build_nrc(uint8_t sid, uds_nrc_t nrc, uint8_t *resp) {
    resp[0] = UDS_NEGATIVE_RESPONSE;
    resp[1] = sid;
    resp[2] = (uint8_t)nrc;
    return 3;
}

static uint16_t rd16_be(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t rd32_be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void wr32_be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static bool is_extended_session(vecu_state_t *s) {
    uint8_t cur = vecu_get_session(s);
    return cur == UDS_SESSION_EXTENDED || cur == UDS_SESSION_PROGRAMMING;
}

/* ---------- 0x10 DiagnosticSessionControl ---------- */

static int h_session_control(vecu_state_t *state,
                             const uint8_t *req, size_t req_len,
                             uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    if (req_len != 2) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }
    uint8_t sub_raw = req[1];
    uint8_t sub = sub_raw & (uint8_t)~UDS_SUPPRESS_POS_RESP_BIT;

    switch (sub) {
        case UDS_SESSION_DEFAULT:
        case UDS_SESSION_PROGRAMMING:
        case UDS_SESSION_EXTENDED:
            vecu_set_session(state, sub);
            break;
        default:
            *resp_len = build_nrc(req[0], UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED, resp);
            return 0;
    }

    if (resp_cap < 6) return -1;
    /* Response = 50 <sub> <P2_max_hi><P2_max_lo> <P2*_max_hi><P2*_max_lo>
     * with P2  = 50ms = 0x0032, P2* = 5000ms/10 = 500 = 0x01F4. */
    resp[0] = UDS_POS_RESP(req[0]);
    resp[1] = sub;
    resp[2] = 0x00; resp[3] = 0x32;
    resp[4] = 0x01; resp[5] = 0xF4;
    *resp_len = 6;

    if (sub_raw & UDS_SUPPRESS_POS_RESP_BIT) return 1;
    return 0;
}

/* ---------- 0x11 ECUReset ---------- */

static int h_ecu_reset(vecu_state_t *state,
                       const uint8_t *req, size_t req_len,
                       uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    (void)resp_cap;
    if (req_len != 2) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }
    uint8_t sub_raw = req[1];
    uint8_t sub = sub_raw & (uint8_t)~UDS_SUPPRESS_POS_RESP_BIT;

    /* 0x01 hardReset / 0x02 keyOffOnReset / 0x03 softReset */
    if (sub != 0x01 && sub != 0x02 && sub != 0x03) {
        *resp_len = build_nrc(req[0], UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED, resp);
        return 0;
    }

    /* "Reset" the soft state: back to default session, security locked,
     * tester-present timer reset. We can't really reboot the process
     * without killing the demo, so this is a logical reset. */
    vecu_set_session(state, UDS_SESSION_DEFAULT);
    pthread_mutex_lock(&state->security.lock);
    state->security.unlocked_level = 0;
    state->security.seed_issued    = false;
    state->security.fail_count     = 0;
    state->security.lock_until_ms  = 0;
    pthread_mutex_unlock(&state->security.lock);

    resp[0] = UDS_POS_RESP(req[0]);
    resp[1] = sub;
    *resp_len = 2;
    if (sub_raw & UDS_SUPPRESS_POS_RESP_BIT) return 1;
    return 0;
}

/* ---------- 0x14 ClearDiagnosticInformation ---------- */

static int h_clear_dtc(vecu_state_t *state,
                       const uint8_t *req, size_t req_len,
                       uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    (void)resp_cap;
    if (req_len != 4) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }
    /* groupOfDTC = 3 bytes. 0xFFFFFF = all groups (only one we support). */
    if (!(req[1] == 0xFF && req[2] == 0xFF && req[3] == 0xFF)) {
        *resp_len = build_nrc(req[0], UDS_NRC_REQUEST_OUT_OF_RANGE, resp);
        return 0;
    }
    (void)dtc_mem_clear_all(&state->dtcs);
    resp[0] = UDS_POS_RESP(req[0]);
    *resp_len = 1;
    return 0;
}

/* ---------- 0x19 ReadDTCInformation ---------- */

static int h_read_dtc(vecu_state_t *state,
                      const uint8_t *req, size_t req_len,
                      uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    if (req_len < 2) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }
    uint8_t sub_raw = req[1];
    uint8_t sub = sub_raw & (uint8_t)~UDS_SUPPRESS_POS_RESP_BIT;

    if (sub != 0x02) {
        /* Only reportDTCByStatusMask is implemented. */
        *resp_len = build_nrc(req[0], UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED, resp);
        return 0;
    }
    if (req_len != 3) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }

    uint8_t mask = req[2];
    dtc_entry_t snap[DTC_MAX];
    size_t n = dtc_mem_snapshot_by_mask(&state->dtcs, mask, snap, DTC_MAX);

    /* Response: 59 02 <availabilityMask> [<dtc3><status1>]*n */
    size_t need = 3 + n * 4;
    if (resp_cap < need) {
        *resp_len = build_nrc(req[0], UDS_NRC_RESPONSE_TOO_LONG, resp);
        return 0;
    }
    resp[0] = UDS_POS_RESP(req[0]);
    resp[1] = sub;
    resp[2] = dtc_mem_availability_mask(&state->dtcs);
    for (size_t i = 0; i < n; ++i) {
        resp[3 + i * 4 + 0] = (uint8_t)((snap[i].dtc >> 16) & 0xFF);
        resp[3 + i * 4 + 1] = (uint8_t)((snap[i].dtc >> 8)  & 0xFF);
        resp[3 + i * 4 + 2] = (uint8_t)(snap[i].dtc & 0xFF);
        resp[3 + i * 4 + 3] = snap[i].status;
    }
    *resp_len = need;
    if (sub_raw & UDS_SUPPRESS_POS_RESP_BIT) return 1;
    return 0;
}

/* ---------- 0x22 ReadDataByIdentifier ---------- */

static int h_read_did(vecu_state_t *state,
                      const uint8_t *req, size_t req_len,
                      uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    /* Spec allows multiple DIDs in one request; we accept exactly one
     * to keep the demo simple. The Python client never asks for more. */
    if (req_len != 3) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }
    uint16_t did = rd16_be(&req[1]);
    const did_entry_t *e = did_table_lookup(&state->dids, did);
    if (e == NULL) {
        *resp_len = build_nrc(req[0], UDS_NRC_REQUEST_OUT_OF_RANGE, resp);
        return 0;
    }

    /* Build payload either from static buffer or callback. */
    uint8_t  payload[256];
    size_t   plen = 0;
    if (e->static_data != NULL) {
        if (e->static_len > sizeof(payload)) return -1;
        memcpy(payload, e->static_data, e->static_len);
        plen = e->static_len;
    } else if (e->read != NULL) {
        if (e->read(payload, sizeof(payload), &plen) != 0) {
            *resp_len = build_nrc(req[0], UDS_NRC_CONDITIONS_NOT_CORRECT, resp);
            return 0;
        }
    } else {
        *resp_len = build_nrc(req[0], UDS_NRC_CONDITIONS_NOT_CORRECT, resp);
        return 0;
    }

    size_t need = 3 + plen;
    if (resp_cap < need) {
        *resp_len = build_nrc(req[0], UDS_NRC_RESPONSE_TOO_LONG, resp);
        return 0;
    }
    resp[0] = UDS_POS_RESP(req[0]);
    resp[1] = (uint8_t)(did >> 8);
    resp[2] = (uint8_t)(did & 0xFF);
    memcpy(&resp[3], payload, plen);
    *resp_len = need;
    return 0;
}

/* ---------- 0x27 SecurityAccess ---------- */

static int h_security_access(vecu_state_t *state,
                             const uint8_t *req, size_t req_len,
                             uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    (void)resp_cap;
    if (req_len < 2) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }
    uint8_t sub_raw = req[1];
    uint8_t sub = sub_raw & (uint8_t)~UDS_SUPPRESS_POS_RESP_BIT;

    /* SecurityAccess only allowed in extended/programming sessions per most
     * OEM mappings (ISO 14229-1 doesn't strictly require it but we enforce). */
    if (!is_extended_session(state)) {
        *resp_len = build_nrc(req[0], UDS_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION, resp);
        return 0;
    }

    /* Odd sub = requestSeed, even = sendKey. */
    if ((sub & 0x01) == 0x01) {
        /* requestSeed */
        if (req_len != 2) {
            *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
            return 0;
        }
        uint32_t seed = 0;
        uint8_t  nrc  = 0;
        int rc = security_request_seed(&state->security, sub, &seed, &nrc);
        if (rc == -1) {
            *resp_len = build_nrc(req[0], (uds_nrc_t)nrc, resp);
            return 0;
        }
        resp[0] = UDS_POS_RESP(req[0]);
        resp[1] = sub;
        wr32_be(&resp[2], seed);
        *resp_len = 6;
        if (sub_raw & UDS_SUPPRESS_POS_RESP_BIT) return 1;
        return 0;
    }

    /* sendKey */
    if (req_len != 6) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }
    uint32_t key = rd32_be(&req[2]);
    uint8_t  nrc = 0;
    /* The "level" for sendKey is sub - 1 (e.g. sub 02 → key for level 01). */
    int rc = security_send_key(&state->security, (uint8_t)(sub - 1), key, &nrc);
    if (rc != 0) {
        *resp_len = build_nrc(req[0], (uds_nrc_t)nrc, resp);
        return 0;
    }
    resp[0] = UDS_POS_RESP(req[0]);
    resp[1] = sub;
    *resp_len = 2;
    if (sub_raw & UDS_SUPPRESS_POS_RESP_BIT) return 1;
    return 0;
}

/* ---------- 0x2E WriteDataByIdentifier ---------- */

static int h_write_did(vecu_state_t *state,
                       const uint8_t *req, size_t req_len,
                       uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    (void)resp_cap;
    if (req_len < 4) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }
    uint16_t did = rd16_be(&req[1]);
    const did_entry_t *e = did_table_lookup(&state->dids, did);
    if (e == NULL || e->write == NULL) {
        *resp_len = build_nrc(req[0], UDS_NRC_REQUEST_OUT_OF_RANGE, resp);
        return 0;
    }
    if (e->requires_extended_session && !is_extended_session(state)) {
        *resp_len = build_nrc(req[0], UDS_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION, resp);
        return 0;
    }
    if (e->requires_security && !security_is_unlocked(&state->security, 0x01)) {
        *resp_len = build_nrc(req[0], UDS_NRC_SECURITY_ACCESS_DENIED, resp);
        return 0;
    }
    if (e->write(&req[3], req_len - 3) != 0) {
        *resp_len = build_nrc(req[0], UDS_NRC_CONDITIONS_NOT_CORRECT, resp);
        return 0;
    }
    resp[0] = UDS_POS_RESP(req[0]);
    resp[1] = (uint8_t)(did >> 8);
    resp[2] = (uint8_t)(did & 0xFF);
    *resp_len = 3;
    return 0;
}

/* ---------- 0x3E TesterPresent ---------- */

static int h_tester_present(vecu_state_t *state,
                            const uint8_t *req, size_t req_len,
                            uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    (void)resp_cap;
    if (req_len != 2) {
        *resp_len = build_nrc(req[0], UDS_NRC_INCORRECT_MESSAGE_LENGTH, resp);
        return 0;
    }
    uint8_t sub_raw = req[1];
    uint8_t sub = sub_raw & (uint8_t)~UDS_SUPPRESS_POS_RESP_BIT;
    if (sub != 0x00) {
        *resp_len = build_nrc(req[0], UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED, resp);
        return 0;
    }

    vecu_mark_tester_present(state);

    resp[0] = UDS_POS_RESP(req[0]);
    resp[1] = sub;
    *resp_len = 2;
    if (sub_raw & UDS_SUPPRESS_POS_RESP_BIT) return 1;
    return 0;
}

/* ---------- dispatch ---------- */

typedef struct {
    uint8_t        sid;
    uds_handler_fn handler;
} dispatch_entry_t;

static const dispatch_entry_t DISPATCH[] = {
    { UDS_SID_DIAGNOSTIC_SESSION_CONTROL, h_session_control  },
    { UDS_SID_ECU_RESET,                  h_ecu_reset        },
    { UDS_SID_CLEAR_DTC,                  h_clear_dtc        },
    { UDS_SID_READ_DTC,                   h_read_dtc         },
    { UDS_SID_READ_DATA_BY_IDENTIFIER,    h_read_did         },
    { UDS_SID_SECURITY_ACCESS,            h_security_access  },
    { UDS_SID_WRITE_DATA_BY_IDENTIFIER,   h_write_did        },
    { UDS_SID_TESTER_PRESENT,             h_tester_present   },
};
#define DISPATCH_N (sizeof(DISPATCH) / sizeof(DISPATCH[0]))

int uds_process_request(vecu_state_t *state,
                        const uint8_t *req, size_t req_len,
                        uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    if (state == NULL || req == NULL || resp == NULL || resp_len == NULL) return -1;
    if (req_len == 0 || resp_cap < 3) return -1;

    uint8_t sid = req[0];
    for (size_t i = 0; i < DISPATCH_N; ++i) {
        if (DISPATCH[i].sid == sid) {
            return DISPATCH[i].handler(state, req, req_len, resp, resp_cap, resp_len);
        }
    }
    *resp_len = build_nrc(sid, UDS_NRC_SERVICE_NOT_SUPPORTED, resp);
    return 0;
}
