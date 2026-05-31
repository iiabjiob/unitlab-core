#include "unitlab_mms_wire_association_fixture.h"

#include <stdlib.h>
#include <string.h>

static void wire_association_fixture_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

void unitlab_mms_wire_association_fixture_init(UnitLabMmsWireAssociationFixture* fixture)
{
    if (fixture == NULL) {
        return;
    }
    memset(fixture, 0, sizeof(*fixture));
    unitlab_mms_transport_frame_init(&fixture->transport);
    unitlab_mms_presentation_apdu_init(&fixture->presentation);
}

int unitlab_mms_wire_association_fixture_encode(const UnitLabMmsWireAssociationFixture* fixture, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t* presentation_scratch = NULL;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsTransportFrame transport_frame;
    size_t presentation_length = 0U;
    size_t transport_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (fixture == NULL || buffer == NULL || encoded_length == NULL) {
        wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "association fixture encode requires fixture, buffer, and encoded_length.");
        return 0;
    }
    if (buffer_length == 0U) {
        wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "association fixture buffer is too small.");
        return 0;
    }
    if (fixture->presentation.kind != UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED && fixture->presentation.kind != UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED) {
        wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "association fixture requires exact X.226 Presentation User-data.");
        return 0;
    }
    if (fixture->presentation.payload_length != 0U && fixture->presentation.payload_bytes == NULL) {
        wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "association fixture presentation payload bytes are required when length is non-zero.");
        return 0;
    }
    if (fixture->transport.cotp.user_data_length != 0U && fixture->transport.cotp.user_data != NULL) {
        wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "association fixture transport user_data must not be prebound.");
        return 0;
    }

    presentation_scratch = (uint8_t*)malloc(buffer_length);
    if (presentation_scratch == NULL) {
        wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "association fixture scratch allocation failed.");
        return 0;
    }

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = fixture->presentation.kind;
    presentation_apdu.payload_bytes = fixture->presentation.payload_bytes;
    presentation_apdu.payload_length = fixture->presentation.payload_length;
    if (!unitlab_mms_presentation_encode(&presentation_apdu, presentation_scratch, buffer_length, &presentation_length, diagnostic)) {
        free(presentation_scratch);
        return 0;
    }

    unitlab_mms_transport_frame_init(&transport_frame);
    transport_frame.cotp = fixture->transport.cotp;
    transport_frame.cotp.user_data = presentation_scratch;
    transport_frame.cotp.user_data_length = presentation_length;
    if (!unitlab_mms_transport_frame_encode(&transport_frame, buffer, buffer_length, &transport_length, diagnostic)) {
        free(presentation_scratch);
        return 0;
    }

    free(presentation_scratch);
    *encoded_length = transport_length;
    wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_wire_association_fixture_decode(UnitLabMmsWireAssociationFixture* fixture, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t transport_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (fixture == NULL || buffer == NULL || consumed_length == NULL) {
        wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "association fixture decode requires fixture, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_wire_association_fixture_init(fixture);
    if (!unitlab_mms_transport_frame_decode(&fixture->transport, buffer, buffer_length, &transport_consumed_length, diagnostic)) {
        return 0;
    }
    if (fixture->transport.cotp.user_data_length == 0U || fixture->transport.cotp.user_data == NULL) {
        wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association fixture is missing presentation bytes.");
        return 0;
    }
    if (!unitlab_mms_presentation_decode(&fixture->presentation, fixture->transport.cotp.user_data, fixture->transport.cotp.user_data_length, &presentation_consumed_length, diagnostic)) {
        return 0;
    }
    if (presentation_consumed_length != fixture->transport.cotp.user_data_length) {
        wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association fixture contains trailing presentation bytes.");
        return 0;
    }
    fixture->encoded_length = transport_consumed_length;
    *consumed_length = transport_consumed_length;
    wire_association_fixture_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
