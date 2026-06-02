#include "unitlab_mms_wire_builder.h"

#include <stdio.h>
#include <string.h>

#include "wire/acse/unitlab_mms_acse.h"
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
    uint8_t application_context_oid_tlv[16U];
    uint8_t application_context_tlv[16U];
    uint8_t result_tlv[16U];
    uint8_t result_source_choice_tlv[16U];
    uint8_t result_source_tlv[16U];
    uint8_t initiate_field0_tlv[16U];
    uint8_t initiate_field1_tlv[16U];
    uint8_t initiate_field2_tlv[16U];
    uint8_t initiate_field3_tlv[16U];
    uint8_t initiate_field4_child0_tlv[16U];
    uint8_t initiate_field4_child1_tlv[16U];
    uint8_t initiate_field4_child2_tlv[32U];
    uint8_t initiate_field4_value[64U];
    uint8_t initiate_field4_tlv[64U];
    uint8_t initiate_response_value[96U];
    uint8_t initiate_response_tlv[128U];
    uint8_t external_indirect_reference_tlv[16U];
    uint8_t external_value[192U];
    uint8_t external_tlv[224U];
    uint8_t user_information_tlv[240U];
    uint8_t aare_value[256U];
    uint8_t aare_tlv[288U];
    size_t application_context_oid_length = 0U;
    size_t application_context_length = 0U;
    size_t result_length = 0U;
    size_t result_source_choice_length = 0U;
    size_t result_source_length = 0U;
    size_t initiate_field0_length = 0U;
    size_t initiate_field1_length = 0U;
    size_t initiate_field2_length = 0U;
    size_t initiate_field3_length = 0U;
    size_t initiate_field4_child0_length = 0U;
    size_t initiate_field4_child1_length = 0U;
    size_t initiate_field4_child2_length = 0U;
    size_t initiate_field4_length = 0U;
    size_t initiate_response_length = 0U;
    size_t external_indirect_reference_length = 0U;
    size_t external_length = 0U;
    size_t user_information_length = 0U;
    size_t aare_length = 0U;
    const uint8_t application_context_oid[] = { 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U };
    const uint8_t result_zero[] = { 0x00U };
    const uint8_t initiate_field0_value_bytes[] = { 0x00U, 0xFDU, 0xE8U };
    const uint8_t initiate_field1_value_bytes[] = { 0x05U };
    const uint8_t initiate_field2_value_bytes[] = { 0x05U };
    const uint8_t initiate_field3_value_bytes[] = { 0x0AU };
    const uint8_t initiate_field4_child0_value_bytes[] = { 0x01U };
    const uint8_t initiate_field4_child1_value_bytes[] = { 0x05U, 0xF1U, 0x00U };
    const uint8_t initiate_field4_child2_value_bytes[] = {
        0x03U, 0xEEU, 0x1CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x18U,
    };
    const uint8_t external_indirect_reference[] = { 0x03U };
    uint8_t presentation_payload[320U];
    uint8_t session_payload[256U];
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsTransportFrame transport_frame;
    size_t presentation_payload_length = 0U;
    size_t session_payload_length = 0U;
    size_t frame_length = 0U;

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

    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 6U, application_context_oid, sizeof(application_context_oid), application_context_oid_tlv, sizeof(application_context_oid_tlv), &application_context_oid_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U, application_context_oid_tlv, application_context_oid_length, application_context_tlv, sizeof(application_context_tlv), &application_context_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U, result_zero, sizeof(result_zero), result_tlv, sizeof(result_tlv), &result_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 1U, result_zero, sizeof(result_zero), result_source_choice_tlv, sizeof(result_source_choice_tlv), &result_source_choice_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 3U, result_source_choice_tlv, result_source_choice_length, result_source_tlv, sizeof(result_source_tlv), &result_source_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U, initiate_field0_value_bytes, sizeof(initiate_field0_value_bytes), initiate_field0_tlv, sizeof(initiate_field0_tlv), &initiate_field0_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 1U, initiate_field1_value_bytes, sizeof(initiate_field1_value_bytes), initiate_field1_tlv, sizeof(initiate_field1_tlv), &initiate_field1_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 2U, initiate_field2_value_bytes, sizeof(initiate_field2_value_bytes), initiate_field2_tlv, sizeof(initiate_field2_tlv), &initiate_field2_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 3U, initiate_field3_value_bytes, sizeof(initiate_field3_value_bytes), initiate_field3_tlv, sizeof(initiate_field3_tlv), &initiate_field3_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 0U, initiate_field4_child0_value_bytes, sizeof(initiate_field4_child0_value_bytes), initiate_field4_child0_tlv, sizeof(initiate_field4_child0_tlv), &initiate_field4_child0_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 1U, initiate_field4_child1_value_bytes, sizeof(initiate_field4_child1_value_bytes), initiate_field4_child1_tlv, sizeof(initiate_field4_child1_tlv), &initiate_field4_child1_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 2U, initiate_field4_child2_value_bytes, sizeof(initiate_field4_child2_value_bytes), initiate_field4_child2_tlv, sizeof(initiate_field4_child2_tlv), &initiate_field4_child2_length, diagnostic)) {
        return 0;
    }
    memcpy(initiate_field4_value, initiate_field4_child0_tlv, initiate_field4_child0_length);
    memcpy(&initiate_field4_value[initiate_field4_child0_length], initiate_field4_child1_tlv, initiate_field4_child1_length);
    memcpy(&initiate_field4_value[initiate_field4_child0_length + initiate_field4_child1_length], initiate_field4_child2_tlv, initiate_field4_child2_length);
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 4U, initiate_field4_value, initiate_field4_child0_length + initiate_field4_child1_length + initiate_field4_child2_length, initiate_field4_tlv, sizeof(initiate_field4_tlv), &initiate_field4_length, diagnostic)) {
        return 0;
    }
    {
        size_t initiate_response_value_length = 0U;
        memcpy(initiate_response_value, initiate_field0_tlv, initiate_field0_length);
        initiate_response_value_length += initiate_field0_length;
        memcpy(&initiate_response_value[initiate_response_value_length], initiate_field1_tlv, initiate_field1_length);
        initiate_response_value_length += initiate_field1_length;
        memcpy(&initiate_response_value[initiate_response_value_length], initiate_field2_tlv, initiate_field2_length);
        initiate_response_value_length += initiate_field2_length;
        memcpy(&initiate_response_value[initiate_response_value_length], initiate_field3_tlv, initiate_field3_length);
        initiate_response_value_length += initiate_field3_length;
        memcpy(&initiate_response_value[initiate_response_value_length], initiate_field4_tlv, initiate_field4_length);
        initiate_response_value_length += initiate_field4_length;
        if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_APPLICATION, 1, 9U, initiate_response_value, initiate_response_value_length, initiate_response_tlv, sizeof(initiate_response_tlv), &initiate_response_length, diagnostic)) {
            return 0;
        }
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 0U, initiate_response_tlv, initiate_response_length, external_tlv, sizeof(external_tlv), &external_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U, external_indirect_reference, sizeof(external_indirect_reference), external_indirect_reference_tlv, sizeof(external_indirect_reference_tlv), &external_indirect_reference_length, diagnostic)) {
        return 0;
    }
    {
        size_t external_value_length = 0U;
        memcpy(external_value, external_indirect_reference_tlv, external_indirect_reference_length);
        external_value_length += external_indirect_reference_length;
        memcpy(&external_value[external_value_length], external_tlv, external_length);
        external_value_length += external_length;
        if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 1, 8U, external_value, external_value_length, external_tlv, sizeof(external_tlv), &external_length, diagnostic)) {
            return 0;
        }
    }
    if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 30U, external_tlv, external_length, user_information_tlv, sizeof(user_information_tlv), &user_information_length, diagnostic)) {
        return 0;
    }
    {
        size_t aare_value_length = 0U;

        memcpy(aare_value, application_context_tlv, application_context_length);
        aare_value_length += application_context_length;
        memcpy(&aare_value[aare_value_length], result_tlv, result_length);
        aare_value_length += result_length;
        memcpy(&aare_value[aare_value_length], result_source_tlv, result_source_length);
        aare_value_length += result_source_length;
        memcpy(&aare_value[aare_value_length], user_information_tlv, user_information_length);
        aare_value_length += user_information_length;
        unitlab_mms_acse_apdu_init(&acse_apdu);
        acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARE;
        acse_apdu.apdu_bytes = aare_value;
        acse_apdu.apdu_length = aare_value_length;
        if (!unitlab_mms_acse_encode(&acse_apdu, aare_tlv, sizeof(aare_tlv), &aare_length, diagnostic)) {
            return 0;
        }
    }

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    presentation_apdu.payload_bytes = aare_tlv;
    presentation_apdu.payload_length = aare_length;
    if (!unitlab_mms_presentation_encode(&presentation_apdu, presentation_payload, sizeof(presentation_payload), &presentation_payload_length, diagnostic)) {
        return 0;
    }

    {
        static const uint8_t session_parameters_prefix[] = {
            0x05U, 0x06U, 0x13U, 0x01U, 0x00U, 0x16U, 0x01U, 0x02U, 0x14U, 0x02U, 0x00U, 0x02U, 0x34U, 0x02U, 0x00U, 0x01U,
        };
        size_t session_parameters_length = sizeof(session_parameters_prefix) + 2U + presentation_payload_length;
        if (session_parameters_length > 0xFFU || session_parameters_length + 2U > sizeof(session_payload)) {
            wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response session payload is too large.");
            return 0;
        }
        session_payload[0U] = 0x0EU;
        session_payload[1U] = (uint8_t)session_parameters_length;
        memcpy(&session_payload[2U], session_parameters_prefix, sizeof(session_parameters_prefix));
        session_payload[2U + sizeof(session_parameters_prefix)] = 0xC1U;
        session_payload[3U + sizeof(session_parameters_prefix)] = (uint8_t)presentation_payload_length;
        memcpy(&session_payload[4U + sizeof(session_parameters_prefix)], presentation_payload, presentation_payload_length);
        session_payload_length = 2U + session_parameters_length;
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
