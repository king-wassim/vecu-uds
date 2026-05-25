#ifndef VECU_UDS_TYPES_H
#define VECU_UDS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define UDS_MAX_MSG_LEN 4095

typedef enum {
    UDS_SID_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    UDS_SID_ECU_RESET                  = 0x11,
    UDS_SID_READ_DATA_BY_IDENTIFIER     = 0x22,
    UDS_SID_WRITE_DATA_BY_IDENTIFIER    = 0x2E,
    UDS_SID_TESTER_PRESENT              = 0x3E,
    UDS_NEGATIVE_RESPONSE               = 0x7F
} uds_sid_t;

typedef enum {
    UDS_NRC_GENERAL_REJECT                  = 0x10,
    UDS_NRC_SERVICE_NOT_SUPPORTED           = 0x11,
    UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED      = 0x12,
    UDS_NRC_INCORRECT_MESSAGE_LENGTH        = 0x13,
    UDS_NRC_CONDITIONS_NOT_CORRECT          = 0x22,
    UDS_NRC_REQUEST_OUT_OF_RANGE            = 0x31
} uds_nrc_t;

#endif
