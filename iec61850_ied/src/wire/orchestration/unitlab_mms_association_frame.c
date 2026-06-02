#include "unitlab_mms_association_frame.h"

#include <stdlib.h>
#include <string.h>

static void association_frame_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

void unitlab_mms_association_frame_init(UnitLabMmsAssociationFrame* frame)
{
    if (frame == NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
    unitlab_mms_transport_frame_init(&frame->transport);
    unitlab_mms_session_spdu_init(&frame->session);
    unitlab_mms_presentation_apdu_init(&frame->presentation);
}

int unitlab_mms_association_frame_encode(const UnitLabMmsAssociationFrame* frame, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t* presentation_scratch = NULL;
    uint8_t* session_scratch = NULL;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsSessionSpdu session_apdu;
    UnitLabMmsTransportFrame transport_frame;
    size_t presentation_length = 0U;
    size_t session_length = 0U;
    size_t transport_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (frame == NULL || buffer == NULL || encoded_length == NULL) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "association frame encode requires frame, buffer, and encoded_length.");
        return 0;
    }
    if (buffer_length == 0U) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "association frame buffer is too small.");
        return 0;
    }
    if (frame->presentation.kind != UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED && frame->presentation.kind != UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "association frame requires exact X.226 Presentation User-data.");
        return 0;
    }
    if (frame->session.kind != UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "association frame requires a data transfer session SPDU.");
        return 0;
    }
    if (frame->presentation.payload_length != 0U && frame->presentation.payload_bytes == NULL) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "association frame presentation payload bytes are required when length is non-zero.");
        return 0;
    }

    presentation_scratch = (uint8_t*)malloc(buffer_length);
    session_scratch = (uint8_t*)malloc(buffer_length);
    if (presentation_scratch == NULL || session_scratch == NULL) {
        free(presentation_scratch);
        free(session_scratch);
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "association frame scratch allocation failed.");
        return 0;
    }

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = frame->presentation.kind;
    presentation_apdu.payload_bytes = frame->presentation.payload_bytes;
    presentation_apdu.payload_length = frame->presentation.payload_length;
    if (!unitlab_mms_presentation_encode(&presentation_apdu, presentation_scratch, buffer_length, &presentation_length, diagnostic)) {
        free(presentation_scratch);
        free(session_scratch);
        return 0;
    }

    if (frame->session.kind != UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER) {
        free(presentation_scratch);
        free(session_scratch);
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "association frame requires a data transfer session SPDU.");
        return 0;
    }

    unitlab_mms_session_spdu_init(&session_apdu);
    session_apdu.kind = frame->session.kind;
    session_apdu.spdu_bytes = presentation_scratch;
    session_apdu.spdu_length = presentation_length;
    if (!unitlab_mms_session_spdu_encode(&session_apdu, session_scratch, buffer_length, &session_length, diagnostic)) {
        free(presentation_scratch);
        free(session_scratch);
        return 0;
    }

    unitlab_mms_transport_frame_init(&transport_frame);
    transport_frame.cotp = frame->transport.cotp;
    transport_frame.cotp.user_data = session_scratch;
    transport_frame.cotp.user_data_length = session_length;
    if (!unitlab_mms_transport_frame_encode(&transport_frame, buffer, buffer_length, &transport_length, diagnostic)) {
        free(presentation_scratch);
        free(session_scratch);
        return 0;
    }

    free(presentation_scratch);
    free(session_scratch);
    *encoded_length = transport_length;
    association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_association_frame_decode(UnitLabMmsAssociationFrame* frame, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (frame == NULL || buffer == NULL || consumed_length == NULL) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "association frame decode requires frame, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_association_frame_init(frame);
    if (!unitlab_mms_transport_frame_decode(&frame->transport, buffer, buffer_length, &transport_consumed_length, diagnostic)) {
        return 0;
    }
    if (frame->transport.cotp.user_data_length == 0U || frame->transport.cotp.user_data == NULL) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association frame is missing session bytes.");
        return 0;
    }
    if (!unitlab_mms_session_spdu_decode(&frame->session, frame->transport.cotp.user_data, frame->transport.cotp.user_data_length, &session_consumed_length, diagnostic)) {
        return 0;
    }
    if (session_consumed_length != frame->transport.cotp.user_data_length) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association frame contains trailing session bytes.");
        return 0;
    }
    if (frame->session.raw_parameter_length == 0U || frame->session.raw_parameter_bytes == NULL) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association frame is missing presentation bytes.");
        return 0;
    }
    if (!unitlab_mms_presentation_decode(&frame->presentation, frame->session.raw_parameter_bytes, frame->session.raw_parameter_length, &presentation_consumed_length, diagnostic)) {
        return 0;
    }
    if (presentation_consumed_length != frame->session.raw_parameter_length) {
        association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association frame contains trailing presentation bytes.");
        return 0;
    }
    frame->encoded_length = transport_consumed_length;
    *consumed_length = transport_consumed_length;
    association_frame_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
