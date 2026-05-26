#ifndef VECU_UDS_H
#define VECU_UDS_H

#include "uds_types.h"
#include "vecu.h"

/*
 * Process one decoded UDS request against the ECU state. The transport
 * layer (ISO-TP) and link layer (CAN) are below us — we only see bytes.
 *
 * Returns:
 *    0  → response built in `resp[0..*resp_len)` (may be a positive or
 *         negative response; both are valid bytes to send).
 *    1  → suppressed positive response (sub-function had the SPR bit set);
 *         caller must NOT send anything.
 *   -1  → invalid arguments / response buffer too small.
 */
int uds_process_request(vecu_state_t *state,
                        const uint8_t *req, size_t req_len,
                        uint8_t *resp, size_t resp_cap, size_t *resp_len);

#endif /* VECU_UDS_H */
