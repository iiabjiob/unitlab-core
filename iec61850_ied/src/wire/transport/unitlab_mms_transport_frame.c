#include "unitlab_mms_transport_frame.h"

#include <string.h>

static void transport_frame_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

void unitlab_mms_transport_frame_init(UnitLabMmsTransportFrame* frame)
{
    if (frame == NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
    unitlab_mms_tpkt_header_init(&frame->tpkt);
    unitlab_mms_cotp_tpdu_init(&frame->cotp);
}

int unitlab_mms_transport_frame_encode(const UnitLabMmsTransportFrame* frame, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t cotp_length = 0U;
    size_t total_length;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (frame == NULL || buffer == NULL || encoded_length == NULL) {
        transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "transport frame encode requires frame, buffer, and encoded_length.");
        return 0;
    }
    if (frame->cotp.user_data_length != 0U && frame->cotp.user_data == NULL) {
        transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "transport frame COTP user data is required when length is non-zero.");
        return 0;
    }
    if (buffer_length < 4U) {
        transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "transport frame buffer is too small.");
        return 0;
    }
    if (!unitlab_mms_cotp_encode(&frame->cotp, &buffer[4], buffer_length - 4U, &cotp_length, diagnostic)) {
        return 0;
    }
    total_length = cotp_length + 4U;
    if (total_length > UINT16_MAX) {
        transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "transport frame is too large.");
        return 0;
    }
    if (!unitlab_mms_tpkt_write_header(buffer, buffer_length, (uint16_t)total_length, diagnostic)) {
        return 0;
    }
    *encoded_length = total_length;
    transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_transport_frame_decode(UnitLabMmsTransportFrame* frame, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    const uint8_t* payload_bytes = NULL;
    size_t payload_length = 0U;
    size_t payload_consumed_length = 0U;
    size_t frame_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (frame == NULL || buffer == NULL || consumed_length == NULL) {
        transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "transport frame decode requires frame, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_transport_frame_init(frame);
    if (!unitlab_mms_tpkt_unwrap(buffer, buffer_length, &payload_bytes, &payload_length, &frame_length, diagnostic)) {
        return 0;
    }
    if (frame_length != buffer_length) {
        transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "transport frame contains trailing bytes.");
        return 0;
    }
    if (payload_length == 0U) {
        transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "transport frame payload is empty.");
        return 0;
    }
    if (!unitlab_mms_cotp_decode(&frame->cotp, payload_bytes, payload_length, &payload_consumed_length, diagnostic)) {
        return 0;
    }
    if (payload_consumed_length != payload_length) {
        transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "transport frame contains trailing COTP bytes.");
        return 0;
    }
    frame->tpkt.version = 3U;
    frame->tpkt.reserved = 0U;
    frame->tpkt.length = (uint16_t)frame_length;
    frame->encoded_length = frame_length;
    *consumed_length = frame_length;
    transport_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
