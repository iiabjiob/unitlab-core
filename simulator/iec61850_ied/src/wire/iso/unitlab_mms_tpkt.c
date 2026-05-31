#include "unitlab_mms_tpkt.h"

#include <string.h>

static void tpkt_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
{
    if (diagnostic == NULL) {
        return;
    }
    diagnostic->code = code;
    if (message == NULL) {
        diagnostic->message[0] = '\0';
        return;
    }
    strncpy(diagnostic->message, message, sizeof(diagnostic->message) - 1U);
    diagnostic->message[sizeof(diagnostic->message) - 1U] = '\0';
}

void unitlab_mms_tpkt_header_init(UnitLabMmsTpktHeader* header)
{
    if (header == NULL) {
        return;
    }
    memset(header, 0, sizeof(*header));
    header->version = 3U;
}

int unitlab_mms_tpkt_wrap(const uint8_t* payload_bytes, size_t payload_length, uint8_t* frame_bytes, size_t frame_capacity, size_t* frame_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint16_t total_length;

    if (frame_length != NULL) {
        *frame_length = 0U;
    }
    if (frame_bytes == NULL || frame_length == NULL) {
        tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "TPKT wrap requires frame bytes and frame length.");
        return 0;
    }
    if (payload_length > (size_t)(UINT16_MAX - 4U)) {
        tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "TPKT payload is too large.");
        return 0;
    }
    if (payload_length != 0U && payload_bytes == NULL) {
        tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "TPKT payload bytes are required when payload length is non-zero.");
        return 0;
    }
    total_length = (uint16_t)(payload_length + 4U);
    if (frame_capacity < (size_t)total_length) {
        tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "TPKT frame buffer is too small.");
        return 0;
    }
    frame_bytes[0] = 3U;
    frame_bytes[1] = 0U;
    frame_bytes[2] = (uint8_t)((total_length >> 8U) & 0xFFU);
    frame_bytes[3] = (uint8_t)(total_length & 0xFFU);
    if (payload_length != 0U && payload_bytes != NULL) {
        memcpy(&frame_bytes[4], payload_bytes, payload_length);
    }
    *frame_length = total_length;
    tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_tpkt_unwrap(const uint8_t* frame_bytes, size_t frame_length, const uint8_t** payload_bytes, size_t* payload_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint16_t total_length;

    if (payload_bytes != NULL) {
        *payload_bytes = NULL;
    }
    if (payload_length != NULL) {
        *payload_length = 0U;
    }
    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (frame_bytes == NULL || payload_bytes == NULL || payload_length == NULL || consumed_length == NULL) {
        tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "TPKT unwrap requires frame bytes and payload outputs.");
        return 0;
    }
    if (frame_length < 4U) {
        tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "TPKT frame is too small.");
        return 0;
    }
    if (frame_bytes[0] != 3U || frame_bytes[1] != 0U) {
        tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "TPKT version or reserved field is invalid.");
        return 0;
    }
    total_length = (uint16_t)(((uint16_t)frame_bytes[2] << 8U) | (uint16_t)frame_bytes[3]);
    if (total_length < 4U) {
        tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "TPKT length is invalid.");
        return 0;
    }
    if (frame_length < total_length) {
        tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "TPKT frame is truncated.");
        return 0;
    }
    *payload_bytes = &frame_bytes[4];
    *payload_length = (size_t)total_length - 4U;
    *consumed_length = total_length;
    tpkt_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
