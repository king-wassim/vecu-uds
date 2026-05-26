#include "uds.h"
#include "vecu.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    vecu_state_t st;
    vecu_init(&st);

    /* TesterPresent positive response. */
    uint8_t  req[]    = { UDS_SID_TESTER_PRESENT, 0x00 };
    uint8_t  resp[16] = {0};
    size_t   resp_len = 0;
    int rc = uds_process_request(&st, req, sizeof(req), resp, sizeof(resp), &resp_len);
    assert(rc == 0);
    assert(resp_len == 2);
    assert(resp[0] == UDS_POS_RESP(UDS_SID_TESTER_PRESENT));
    assert(resp[1] == 0x00);

    /* Unknown SID → NRC 0x11. */
    uint8_t bad[] = { 0xAB };
    rc = uds_process_request(&st, bad, sizeof(bad), resp, sizeof(resp), &resp_len);
    assert(rc == 0);
    assert(resp_len == 3);
    assert(resp[0] == UDS_NEGATIVE_RESPONSE);
    assert(resp[1] == 0xAB);
    assert(resp[2] == UDS_NRC_SERVICE_NOT_SUPPORTED);

    /* ReadDataByIdentifier 0xF190 (VIN) → 17 bytes payload. */
    uint8_t  vinreq[] = { UDS_SID_READ_DATA_BY_IDENTIFIER, 0xF1, 0x90 };
    uint8_t  big[64]  = {0};
    rc = uds_process_request(&st, vinreq, sizeof(vinreq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(resp_len == 3 + 17);
    assert(big[0] == UDS_POS_RESP(UDS_SID_READ_DATA_BY_IDENTIFIER));
    assert(big[1] == 0xF1 && big[2] == 0x90);
    assert(big[3] == 'V' && big[4] == 'F');

    /* ReadDataByIdentifier unknown DID → NRC 0x31. */
    uint8_t  unkreq[] = { UDS_SID_READ_DATA_BY_IDENTIFIER, 0xAB, 0xCD };
    rc = uds_process_request(&st, unkreq, sizeof(unkreq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(big[0] == UDS_NEGATIVE_RESPONSE);
    assert(big[2] == UDS_NRC_REQUEST_OUT_OF_RANGE);

    /* WriteDID without extended session → NRC 0x7F. */
    uint8_t  wreq[]   = { UDS_SID_WRITE_DATA_BY_IDENTIFIER, 0xF2, 0x00, 'X' };
    rc = uds_process_request(&st, wreq, sizeof(wreq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(big[0] == UDS_NEGATIVE_RESPONSE);
    assert(big[2] == UDS_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);

    /* Switch to extended, then WriteDID without security → NRC 0x33. */
    uint8_t  sreq[]   = { UDS_SID_DIAGNOSTIC_SESSION_CONTROL, 0x03 };
    rc = uds_process_request(&st, sreq, sizeof(sreq), big, sizeof(big), &resp_len);
    assert(rc == 0 && big[0] == UDS_POS_RESP(UDS_SID_DIAGNOSTIC_SESSION_CONTROL));

    rc = uds_process_request(&st, wreq, sizeof(wreq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(big[0] == UDS_NEGATIVE_RESPONSE);
    assert(big[2] == UDS_NRC_SECURITY_ACCESS_DENIED);

    /* ReadDTC sub 0x02 mask 0x09 → at least the two seeded codes. */
    uint8_t  dreq[]   = { UDS_SID_READ_DTC, 0x02, 0x09 };
    rc = uds_process_request(&st, dreq, sizeof(dreq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(big[0] == UDS_POS_RESP(UDS_SID_READ_DTC));
    assert(big[1] == 0x02);
    /* 3 header bytes + 2 DTCs * 4 bytes = 11 */
    assert(resp_len == 3 + 2 * 4);

    /* ClearDTC all groups. */
    uint8_t  creq[]   = { UDS_SID_CLEAR_DTC, 0xFF, 0xFF, 0xFF };
    rc = uds_process_request(&st, creq, sizeof(creq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(big[0] == UDS_POS_RESP(UDS_SID_CLEAR_DTC));
    assert(resp_len == 1);

    /* ReadDTC now returns 0 DTCs. */
    rc = uds_process_request(&st, dreq, sizeof(dreq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(resp_len == 3);

    /* SecurityAccess seed → key → unlocked. */
    uint8_t  seedreq[] = { UDS_SID_SECURITY_ACCESS, 0x01 };
    rc = uds_process_request(&st, seedreq, sizeof(seedreq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(big[0] == UDS_POS_RESP(UDS_SID_SECURITY_ACCESS));
    assert(big[1] == 0x01);
    assert(resp_len == 6);

    uint32_t seed = ((uint32_t)big[2] << 24) | ((uint32_t)big[3] << 16) |
                    ((uint32_t)big[4] << 8)  |  (uint32_t)big[5];
    uint32_t key  = seed ^ 0xDEADBEEFu;
    uint8_t  keyreq[] = {
        UDS_SID_SECURITY_ACCESS, 0x02,
        (uint8_t)(key >> 24), (uint8_t)(key >> 16),
        (uint8_t)(key >> 8),  (uint8_t)key
    };
    rc = uds_process_request(&st, keyreq, sizeof(keyreq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(big[0] == UDS_POS_RESP(UDS_SID_SECURITY_ACCESS));
    assert(big[1] == 0x02);

    /* Now WriteDID succeeds. */
    rc = uds_process_request(&st, wreq, sizeof(wreq), big, sizeof(big), &resp_len);
    assert(rc == 0);
    assert(big[0] == UDS_POS_RESP(UDS_SID_WRITE_DATA_BY_IDENTIFIER));

    vecu_destroy(&st);
    puts("OK");
    return 0;
}
