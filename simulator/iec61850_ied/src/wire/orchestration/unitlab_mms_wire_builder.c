#include "unitlab_mms_wire_builder.h"

#include <stdio.h>

#include "wire/transport/unitlab_mms_wire_association_fixture.h"

static void wire_builder_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
{
    if (diagnostic == NULL) {
        return;
    }
    diagnostic->code = code;
    if (message == NULL) {
        diagnostic->message[0] = '\0';
        return;
    }
    snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
}

static int unitlab_mms_build_cotp_dt_from_session_bytes(
    const uint8_t* session_bytes,
    size_t session_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsTransportFrame transport_frame;
    size_t frame_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (session_bytes == NULL || session_length == 0U || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "COTP DT frame build requires session bytes, buffer, and encoded_length.");
        return 0;
    }

    unitlab_mms_transport_frame_init(&transport_frame);
    transport_frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    transport_frame.cotp.eot = 1;
    transport_frame.cotp.user_data = session_bytes;
    transport_frame.cotp.user_data_length = session_length;

    if (!unitlab_mms_transport_frame_encode(&transport_frame, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }

    *encoded_length = frame_length;
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

static int unitlab_mms_build_cotp_connect_frame(
    UnitLabMmsCotpTpduKind kind,
    uint16_t destination_reference,
    uint16_t source_reference,
    uint8_t tpdu_class,
    const uint8_t* parameters,
    size_t parameters_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsTransportFrame transport_frame;
    size_t frame_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "COTP connect frame build requires buffer and encoded_length.");
        return 0;
    }
    if (buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "COTP connect frame buffer must be non-zero.");
        return 0;
    }
    if (parameters_length != 0U && parameters == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "COTP connect frame parameters are required when parameters_length is non-zero.");
        return 0;
    }

    unitlab_mms_transport_frame_init(&transport_frame);
    transport_frame.cotp.kind = kind;
    transport_frame.cotp.destination_reference = destination_reference;
    transport_frame.cotp.source_reference = source_reference;
    transport_frame.cotp.tpdu_class = tpdu_class;
    transport_frame.cotp.user_data = parameters;
    transport_frame.cotp.user_data_length = parameters_length;
    if (!unitlab_mms_transport_frame_encode(&transport_frame, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }

    *encoded_length = frame_length;
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_build_cotp_connect_request_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    static const uint8_t parameters[] = { 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };
    return unitlab_mms_build_cotp_connect_frame(
        UNITLAB_MMS_COTP_TPDU_CR,
        0U,
        1U,
        0U,
        parameters,
        sizeof(parameters),
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

int unitlab_mms_build_cotp_connect_response_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    static const uint8_t parameters[] = { 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };
    return unitlab_mms_build_cotp_connect_frame(
        UNITLAB_MMS_COTP_TPDU_CC,
        1U,
        1U,
        0U,
        parameters,
        sizeof(parameters),
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

int unitlab_mms_build_wire_frame_from_pdu(
    const UnitLabMmsPdu* pdu,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsWireAssociationFixture fixture;
    size_t payload_length = 0U;
    size_t frame_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (pdu == NULL || scratch == NULL || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Wire frame build requires a PDU, scratch buffer, buffer, and encoded_length.");
        return 0;
    }
    if (scratch_length == 0U || buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Wire frame build scratch and buffer must be non-zero.");
        return 0;
    }

    if (!unitlab_mms_pdu_encode(pdu, scratch, scratch_length, &payload_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    fixture.presentation.payload_bytes = scratch;
    fixture.presentation.payload_length = payload_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    if (!unitlab_mms_wire_association_fixture_encode(&fixture, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }

    *encoded_length = frame_length;
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}


int unitlab_mms_build_association_response_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    /*
     * Minimal association-accept smoke frame.
     *
     * Important:
     * This intentionally does NOT use unitlab_mms_wire_association_fixture_encode(),
     * because that fixture is only for normal Session DATA TRANSFER:
     *
     *   01 00 01 00 ...
     *
     * Association response must not be sent as ordinary Data Transfer.
     *
     * Current goal:
     * - stop advertising AARE as Session DATA TRANSFER;
     * - move the Wireshark boundary to Session ACCEPT / Presentation CPA area;
     * - keep AARE bytes visible in the association response path.
     *
     * This is still a smoke path, not the final industrial ACSE/MMS association encoder.
     */
    static const uint8_t session_accept_smoke_spdu[] = {
        /*
         * Session ACCEPT SPDU.
         * SI = 0x0E.
         *
         * The bytes after the LI are currently a minimal raw association accept
         * payload carrying a Presentation/ACSE-looking response.
         *
         * If Wireshark now stops at Session ACCEPT, the next task is to replace
         * this raw body with a proper X.226 CPA PPDU.
         */
        0x0EU, 0x4FU,

        /*
         * Current raw upper payload.
         * This keeps the AARE bytes present in the server association response.
         * AARE starts at 0x61 0x4A.
         *
         * NOTE:
         * This is intentionally not wrapped as Presentation simply-encoded-data
         * through Session DATA TRANSFER anymore.
         */
        0x61U, 0x4AU,
        0xA1U, 0x07U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x12U, 0x02U, 0x03U,
        0xA2U, 0x03U, 0x02U, 0x01U, 0x00U,
        0xA3U, 0x05U, 0xA1U, 0x03U, 0x02U, 0x01U, 0x00U,
        0xBEU, 0x33U,
        0x28U, 0x31U,
        0x06U, 0x02U, 0x52U, 0x01U,
        0x02U, 0x01U, 0x03U,
        0xA0U, 0x28U,
        0xA9U, 0x26U,
        0x80U, 0x03U, 0x00U, 0xFAU, 0x00U,
        0x81U, 0x01U, 0x0AU,
        0x82U, 0x01U, 0x0AU,
        0x83U, 0x01U, 0x05U,
        0xA4U, 0x16U,
        0x80U, 0x01U, 0x01U,
        0x81U, 0x03U, 0x05U, 0xE1U, 0x00U,
        0x82U, 0x0CU, 0x03U, 0xA0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0xEDU, 0x10U,
    };

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response frame requires buffer and encoded_length.");
        return 0;
    }
    if (buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response frame buffer must be non-zero.");
        return 0;
    }

    return unitlab_mms_build_cotp_dt_from_session_bytes(
        session_accept_smoke_spdu,
        sizeof(session_accept_smoke_spdu),
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

int unitlab_mms_build_confirmed_response_frame(
    const UnitLabMmsPdu* response_pdu,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (response_pdu == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Confirmed response frame requires a response PDU.");
        return 0;
    }
    if (response_pdu->kind != UNITLAB_MMS_PDU_CONFIRMED_RESPONSE) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Confirmed response frame requires a confirmed response PDU.");
        return 0;
    }
    return unitlab_mms_build_wire_frame_from_pdu(response_pdu, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
}
