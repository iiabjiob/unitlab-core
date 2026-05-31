#include "unitlab_mms_wire_envelope.h"

#include <string.h>

static void wire_envelope_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

void unitlab_mms_wire_envelope_init(UnitLabMmsWireEnvelope* envelope)
{
    if (envelope == NULL) {
        return;
    }
    memset(envelope, 0, sizeof(*envelope));
    unitlab_mms_transport_frame_init(&envelope->transport);
    unitlab_mms_presentation_apdu_init(&envelope->presentation);
    unitlab_mms_acse_apdu_init(&envelope->acse);
    unitlab_mms_pdu_init(&envelope->pdu);
}

int unitlab_mms_wire_envelope_decode(UnitLabMmsWireEnvelope* envelope, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    const uint8_t* presentation_bytes = NULL;
    size_t presentation_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    size_t acse_consumed_length = 0U;
    size_t pdu_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (envelope == NULL || buffer == NULL || consumed_length == NULL) {
        wire_envelope_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "wire envelope decode requires envelope, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_wire_envelope_init(envelope);
    if (!unitlab_mms_transport_frame_decode(&envelope->transport, buffer, buffer_length, &transport_consumed_length, diagnostic)) {
        return 0;
    }
    if (envelope->transport.cotp.user_data_length == 0U || envelope->transport.cotp.user_data == NULL) {
        wire_envelope_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "wire envelope is missing presentation bytes.");
        return 0;
    }
    if (!unitlab_mms_presentation_decode(&envelope->presentation, envelope->transport.cotp.user_data, envelope->transport.cotp.user_data_length, &presentation_consumed_length, diagnostic)) {
        return 0;
    }
    if (presentation_consumed_length != envelope->transport.cotp.user_data_length) {
        wire_envelope_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "wire envelope contains trailing presentation bytes.");
        return 0;
    }
    presentation_bytes = envelope->presentation.payload_bytes;
    presentation_length = envelope->presentation.payload_length;
    if (presentation_length == 0U || presentation_bytes == NULL) {
        wire_envelope_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "wire envelope is missing ACSE bytes.");
        return 0;
    }
    if (!unitlab_mms_acse_decode(&envelope->acse, presentation_bytes, presentation_length, &acse_consumed_length, diagnostic)) {
        return 0;
    }
    if (acse_consumed_length != presentation_length) {
        wire_envelope_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "wire envelope contains trailing ACSE bytes.");
        return 0;
    }
    if (envelope->acse.apdu_length == 0U || envelope->acse.apdu_bytes == NULL) {
        wire_envelope_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "wire envelope is missing MMS bytes.");
        return 0;
    }
    if (!unitlab_mms_pdu_decode(&envelope->pdu, envelope->acse.apdu_bytes, envelope->acse.apdu_length, &pdu_consumed_length, diagnostic)) {
        return 0;
    }
    if (pdu_consumed_length != envelope->acse.apdu_length) {
        wire_envelope_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "wire envelope contains trailing MMS bytes.");
        return 0;
    }
    envelope->encoded_length = transport_consumed_length;
    *consumed_length = transport_consumed_length;
    wire_envelope_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
