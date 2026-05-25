#include "uds.h"
#include <string.h>

static size_t build_nrc(uint8_t sid, uds_nrc_t nrc, uint8_t *resp) {
    resp[0] = UDS_NEGATIVE_RESPONSE;
    resp[1] = sid;
    resp[2] = (uint8_t)nrc;
    return 3;
}

int uds_process_request(const uint8_t *req, size_t req_len,
                        uint8_t *resp, size_t resp_cap, size_t *resp_len) {
    if (req == NULL || resp == NULL || resp_len == NULL) return -1;
    if (req_len == 0 || resp_cap < 3) return -1;

    uint8_t sid = req[0];

    switch (sid) {
        case UDS_SID_TESTER_PRESENT:
            if (resp_cap < 2) return -1;
            resp[0] = (uint8_t)(sid + 0x40);
            resp[1] = (req_len >= 2) ? req[1] : 0x00;
            *resp_len = 2;
            return 0;

        default:
            *resp_len = build_nrc(sid, UDS_NRC_SERVICE_NOT_SUPPORTED, resp);
            return 0;
    }
}
