#include "uds.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    uint8_t req[]  = { UDS_SID_TESTER_PRESENT, 0x00 };
    uint8_t resp[8] = {0};
    size_t resp_len = 0;

    int rc = uds_process_request(req, sizeof(req), resp, sizeof(resp), &resp_len);
    assert(rc == 0);
    assert(resp_len == 2);
    assert(resp[0] == 0x7E);

    uint8_t bad[]  = { 0xAB };
    rc = uds_process_request(bad, sizeof(bad), resp, sizeof(resp), &resp_len);
    assert(rc == 0);
    assert(resp_len == 3);
    assert(resp[0] == UDS_NEGATIVE_RESPONSE);
    assert(resp[2] == UDS_NRC_SERVICE_NOT_SUPPORTED);

    puts("OK");
    return 0;
}
