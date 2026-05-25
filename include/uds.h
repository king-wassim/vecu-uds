#ifndef VECU_UDS_H
#define VECU_UDS_H

#include "uds_types.h"

int uds_process_request(const uint8_t *req, size_t req_len,
                        uint8_t *resp, size_t resp_cap, size_t *resp_len);

#endif
