#ifndef VECU_UDS_TYPES_H
#define VECU_UDS_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* Largest UDS message (request or response) we will ever assemble.
 * Bounded by ISO-TP segmentation limit (ISO 15765-2 §6.5.3.3). */
#define UDS_MAX_MSG_LEN 4095

/* Positive response = request SID + 0x40 (ISO 14229-1 §7.6) */
#define UDS_POS_RESP(sid) ((uint8_t)((sid) + 0x40))

/* Service IDs we implement (subset of ISO 14229-1 Annex A) */
typedef enum {
    UDS_SID_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    UDS_SID_ECU_RESET                  = 0x11,
    UDS_SID_CLEAR_DTC                  = 0x14,
    UDS_SID_READ_DTC                   = 0x19,
    UDS_SID_READ_DATA_BY_IDENTIFIER    = 0x22,
    UDS_SID_SECURITY_ACCESS            = 0x27,
    UDS_SID_WRITE_DATA_BY_IDENTIFIER   = 0x2E,
    UDS_SID_TESTER_PRESENT             = 0x3E,
    UDS_NEGATIVE_RESPONSE              = 0x7F
} uds_sid_t;

/* Diagnostic sessions (ISO 14229-1 §9.2.2.2) */
typedef enum {
    UDS_SESSION_DEFAULT       = 0x01,
    UDS_SESSION_PROGRAMMING   = 0x02,
    UDS_SESSION_EXTENDED      = 0x03,
    UDS_SESSION_SAFETY_SYSTEM = 0x04
} uds_session_t;

/* Negative response codes (ISO 14229-1 Annex A.1) */
typedef enum {
    UDS_NRC_GENERAL_REJECT                        = 0x10,
    UDS_NRC_SERVICE_NOT_SUPPORTED                 = 0x11,
    UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED            = 0x12,
    UDS_NRC_INCORRECT_MESSAGE_LENGTH              = 0x13,
    UDS_NRC_RESPONSE_TOO_LONG                     = 0x14,
    UDS_NRC_BUSY_REPEAT_REQUEST                   = 0x21,
    UDS_NRC_CONDITIONS_NOT_CORRECT                = 0x22,
    UDS_NRC_REQUEST_SEQUENCE_ERROR                = 0x24,
    UDS_NRC_REQUEST_OUT_OF_RANGE                  = 0x31,
    UDS_NRC_SECURITY_ACCESS_DENIED                = 0x33,
    UDS_NRC_INVALID_KEY                           = 0x35,
    UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS           = 0x36,
    UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED       = 0x37,
    UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED          = 0x70,
    UDS_NRC_GENERAL_PROGRAMMING_FAILURE           = 0x72,
    UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED_IN_SESSION = 0x7E,
    UDS_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION      = 0x7F
} uds_nrc_t;

/* Sub-function "suppress positive response" mask (ISO 14229-1 §7.5.4) */
#define UDS_SUPPRESS_POS_RESP_BIT 0x80

#endif /* VECU_UDS_TYPES_H */
