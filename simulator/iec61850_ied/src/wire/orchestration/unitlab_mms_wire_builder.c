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
    static const uint8_t reference_association_response_user_data[] = {
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
    UnitLabMmsTransportFrame transport_frame;
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

    unitlab_mms_transport_frame_init(&transport_frame);
    transport_frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    transport_frame.cotp.eot = 1;
    transport_frame.cotp.user_data = reference_association_response_user_data;
    transport_frame.cotp.user_data_length = sizeof(reference_association_response_user_data);
    if (!unitlab_mms_transport_frame_encode(&transport_frame, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }
    *encoded_length = frame_length;
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

static int wire_builder_encode_nested_element(
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
    return wire_builder_encode_ber_element(tag_class, constructed, tag_number, value_bytes, value_length, buffer, buffer_length, encoded_length, diagnostic);
}

int unitlab_mms_build_information_report_frame(
    const char* item_id,
    uint8_t boolean_value,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    const char* effective_item_id = (item_id != NULL && item_id[0] != '\0') ? item_id : "RPT";
    uint8_t item_id_bytes[64U];
    uint8_t variable_spec_bytes[80U];
    uint8_t sequence_bytes[96U];
    uint8_t list_of_variables_bytes[112U];
    uint8_t access_result_bytes[8U];
    uint8_t list_of_access_results_bytes[16U];
    uint8_t report_content_bytes[160U];
    uint8_t report_pdu_bytes[192U];
    size_t item_id_length = 0U;
    size_t variable_spec_length = 0U;
    size_t sequence_length = 0U;
    size_t list_of_variables_length = 0U;
    size_t access_result_length = 0U;
    size_t list_of_access_results_length = 0U;
    size_t report_content_length = 0U;
    size_t report_pdu_length = 0U;
    UnitLabMmsPdu report_pdu;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (scratch == NULL || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Information report frame requires scratch, buffer, and encoded_length.");
        return 0;
    }
    if (scratch_length == 0U || buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Information report frame scratch and buffer must be non-zero.");
        return 0;
    }

    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            0U,
            (const uint8_t*)effective_item_id,
            strlen(effective_item_id),
            item_id_bytes,
            sizeof(item_id_bytes),
            &item_id_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            item_id_bytes,
            item_id_length,
            variable_spec_bytes,
            sizeof(variable_spec_bytes),
            &variable_spec_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            16U,
            variable_spec_bytes,
            variable_spec_length,
            sequence_bytes,
            sizeof(sequence_bytes),
            &sequence_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            sequence_bytes,
            sequence_length,
            list_of_variables_bytes,
            sizeof(list_of_variables_bytes),
            &list_of_variables_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            3U,
            &boolean_value,
            1U,
            access_result_bytes,
            sizeof(access_result_bytes),
            &access_result_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            access_result_bytes,
            access_result_length,
            list_of_access_results_bytes,
            sizeof(list_of_access_results_bytes),
            &list_of_access_results_length,
            diagnostic)) {
        return 0;
    }
    if (list_of_variables_length + list_of_access_results_length > sizeof(report_content_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Information report content buffer is too small.");
        return 0;
    }
    memcpy(report_content_bytes, list_of_variables_bytes, list_of_variables_length);
    memcpy(report_content_bytes + list_of_variables_length, list_of_access_results_bytes, list_of_access_results_length);
    report_content_length = list_of_variables_length + list_of_access_results_length;
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            3U,
            report_content_bytes,
            report_content_length,
            report_pdu_bytes,
            sizeof(report_pdu_bytes),
            &report_pdu_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_pdu_init(&report_pdu);
    report_pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    report_pdu.has_service = 1;
    report_pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    report_pdu.pdu_bytes = report_pdu_bytes;
    report_pdu.pdu_length = report_pdu_length;
    return unitlab_mms_build_wire_frame_from_pdu(&report_pdu, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
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
