#include "unitlab_mms_wire_builder.h"

#include <stdio.h>
#include <string.h>

#include "wire/ber/unitlab_mms_ber.h"
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

static int wire_builder_encode_ber_element(
    UnitLabMmsBerTagClass tag_class,
    int constructed,
    uint32_t tag_number,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = tag_class;
    element.tag.constructed = constructed;
    element.tag.tag_number = tag_number;
    element.value_bytes = value_bytes;
    element.value_length = value_length;
    return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
}

int unitlab_mms_build_reference_first_read_response_payload(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t leaf_tlv[16U];
    uint8_t a7_tlv[16U];
    uint8_t a1_tlv[32U];
    uint8_t a4_tlv[32U];
    uint8_t a1_inner_tlv[48U];
    uint8_t a0_tlv[64U];
    uint8_t sequence_tlv[80U];
    uint8_t integer_tlv[8U];
    size_t leaf_length = 0U;
    size_t a7_length = 0U;
    size_t a1_length = 0U;
    size_t a4_length = 0U;
    size_t a1_inner_length = 0U;
    size_t a0_length = 0U;
    size_t sequence_length = 0U;
    size_t integer_length = 0U;
    size_t apdu_length = 0U;
    const uint8_t leaf_value[] = { 0x08U, 0xBFU, 0x7EU, 0x96U, 0x18U };

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Reference first read response payload requires buffer and encoded_length.");
        return 0;
    }

    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 7U, leaf_value, sizeof(leaf_value), leaf_tlv, sizeof(leaf_tlv), &leaf_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U, leaf_tlv, leaf_length, a7_tlv, sizeof(a7_tlv), &a7_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 4U, a7_tlv, a7_length, a4_tlv, sizeof(a4_tlv), &a4_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U, a4_tlv, a4_length, a1_tlv, sizeof(a1_tlv), &a1_length, diagnostic)) {
        return 0;
    }
    {
        const uint8_t integer_one[] = { 0x01U };
        if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U, integer_one, sizeof(integer_one), integer_tlv, sizeof(integer_tlv), &integer_length, diagnostic)) {
            return 0;
        }
    }
    {
        uint8_t inner_value[64U];
        memcpy(inner_value, integer_tlv, integer_length);
        memcpy(&inner_value[integer_length], a1_tlv, a1_length);
        if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U, inner_value, integer_length + a1_length, a1_inner_tlv, sizeof(a1_inner_tlv), &a1_inner_length, diagnostic)) {
            return 0;
        }
    }
    {
        uint8_t outer_value[80U];
        memcpy(outer_value, a1_inner_tlv, a1_inner_length);
        if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 0U, outer_value, a1_inner_length, a0_tlv, sizeof(a0_tlv), &a0_length, diagnostic)) {
            return 0;
        }
    }
    {
        const uint8_t integer_three[] = { 0x03U };
        if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U, integer_three, sizeof(integer_three), integer_tlv, sizeof(integer_tlv), &integer_length, diagnostic)) {
            return 0;
        }
    }
    {
        uint8_t sequence_value[96U];
        memcpy(sequence_value, integer_tlv, integer_length);
        memcpy(&sequence_value[integer_length], a0_tlv, a0_length);
        if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 1, 16U, sequence_value, integer_length + a0_length, sequence_tlv, sizeof(sequence_tlv), &sequence_length, diagnostic)) {
            return 0;
        }
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_APPLICATION, 1, 1U, sequence_tlv, sequence_length, buffer, buffer_length, &apdu_length, diagnostic)) {
        return 0;
    }
    *encoded_length = apdu_length;
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_build_reference_first_read_response_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t aare_apdu[64U];
    uint8_t session_payload[96U];
    UnitLabMmsSessionSpdu session_apdu;
    UnitLabMmsTransportFrame transport_frame;
    size_t aare_apdu_length = 0U;
    size_t session_payload_length = 0U;
    size_t frame_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Reference first read response frame requires buffer and encoded_length.");
        return 0;
    }
    if (!unitlab_mms_build_reference_first_read_response_payload(aare_apdu, sizeof(aare_apdu), &aare_apdu_length, diagnostic)) {
        return 0;
    }
    unitlab_mms_session_spdu_init(&session_apdu);
    session_apdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_apdu.spdu_bytes = aare_apdu;
    session_apdu.spdu_length = aare_apdu_length;
    if (!unitlab_mms_session_spdu_encode(&session_apdu, session_payload, sizeof(session_payload), &session_payload_length, diagnostic)) {
        return 0;
    }
    unitlab_mms_transport_frame_init(&transport_frame);
    transport_frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    transport_frame.cotp.eot = 1;
    transport_frame.cotp.user_data = session_payload;
    transport_frame.cotp.user_data_length = session_payload_length;
    if (!unitlab_mms_transport_frame_encode(&transport_frame, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }
    *encoded_length = frame_length;
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
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
     * Reference-shaped association response frame.
     *
     * This uses the captured libiec61850 response bytes for the server-side
     * association response path so Wireshark can decode the AARE at the correct
     * boundary after the client sends AARQ.
     */
    static const uint8_t session_association_response_spdu[] = {
        0x0EU, 0x86U, 0x05U, 0x06U, 0x13U, 0x01U, 0x00U, 0x16U, 0x01U, 0x02U, 0x14U, 0x02U, 0x00U, 0x02U, 0x34U, 0x02U,
        0x00U, 0x01U, 0xC1U, 0x74U, 0x31U, 0x72U, 0xA0U, 0x03U, 0x80U, 0x01U, 0x01U, 0xA2U, 0x6BU, 0x83U, 0x04U, 0x00U,
        0x00U, 0x00U, 0x01U, 0xA5U, 0x12U, 0x30U, 0x07U, 0x80U, 0x01U, 0x00U, 0x81U, 0x02U, 0x51U, 0x01U, 0x30U, 0x07U,
        0x80U, 0x01U, 0x00U, 0x81U, 0x02U, 0x51U, 0x01U, 0x61U, 0x4FU, 0x30U, 0x4DU, 0x02U, 0x01U, 0x01U, 0xA0U, 0x48U,
        0x61U, 0x46U, 0xA1U, 0x07U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U, 0xA2U, 0x03U, 0x02U, 0x01U, 0x00U,
        0xA3U, 0x05U, 0xA1U, 0x03U, 0x02U, 0x01U, 0x00U, 0xBEU, 0x2FU, 0x28U, 0x2DU, 0x02U, 0x01U, 0x03U, 0xA0U, 0x28U,
        0xA9U, 0x26U, 0x80U, 0x03U, 0x00U, 0xFDU, 0xE8U, 0x81U, 0x01U, 0x05U, 0x82U, 0x01U, 0x05U, 0x83U, 0x01U, 0x0AU,
        0xA4U, 0x16U, 0x80U, 0x01U, 0x01U, 0x81U, 0x03U, 0x05U, 0xF1U, 0x00U, 0x82U, 0x0CU, 0x03U, 0xEEU, 0x1CU, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x18U,
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
        session_association_response_spdu,
        sizeof(session_association_response_spdu),
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
