#include "vecu.h"
#include "uds.h"
#include <stdio.h>

int main(void) {
    vecu_state_t state;
    vecu_init(&state);

    uint8_t req[]  = { UDS_SID_TESTER_PRESENT, 0x00 };
    uint8_t resp[8] = {0};
    size_t resp_len = 0;

    if (uds_process_request(req, sizeof(req), resp, sizeof(resp), &resp_len) != 0) {
        fprintf(stderr, "uds_process_request failed\n");
        return 1;
    }

    printf("vecu-uds v0.1.0 — session=0x%02X\n", state.session);
    printf("response (%zu bytes):", resp_len);
    for (size_t i = 0; i < resp_len; ++i) printf(" %02X", resp[i]);
    printf("\n");
    return 0;
}
