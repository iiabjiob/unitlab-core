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
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsTransportFrame transport_frame;
    uint8_t session_bytes[512U];
    size_t payload_length = 0U;
    size_t session_length = 0U;
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

    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_spdu.spdu_bytes = scratch;
    session_spdu.spdu_length = payload_length;
    if (!unitlab_mms_session_spdu_encode(&session_spdu, session_bytes, sizeof(session_bytes), &session_length, diagnostic)) {
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


static int wire_builder_build_association_response_frame_structured(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_association_response_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    return wire_builder_build_association_response_frame_structured(buffer, buffer_length, encoded_length, diagnostic);
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

static int wire_builder_append_bytes(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* offset,
    const uint8_t* bytes,
    size_t bytes_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (buffer == NULL || offset == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response builder requires valid output buffers.");
        return 0;
    }
    if (bytes_length != 0U && bytes == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response builder requires value bytes when length is non-zero.");
        return 0;
    }
    if (*offset > buffer_length || bytes_length > buffer_length - *offset) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response builder buffer is too small.");
        return 0;
    }
    if (bytes_length != 0U) {
        memcpy(&buffer[*offset], bytes, bytes_length);
    }
    *offset += bytes_length;
    return 1;
}

static int wire_builder_append_short_tlv(
    uint8_t tag,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* offset,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (value_length > 0xFFU) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response builder TLV is too large.");
        return 0;
    }
    if (buffer == NULL || offset == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response builder requires valid output buffers.");
        return 0;
    }
    if (*offset > buffer_length || (size_t)2U + value_length > buffer_length - *offset) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response builder buffer is too small.");
        return 0;
    }
    buffer[*offset] = tag;
    buffer[*offset + 1U] = (uint8_t)value_length;
    *offset += 2U;
    return wire_builder_append_bytes(buffer, buffer_length, offset, value_bytes, value_length, diagnostic);
}

static int wire_builder_build_initiate_response_detail(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t max_pdu_size_field[16U];
    uint8_t max_serv_out_calling_field[16U];
    uint8_t max_serv_out_called_field[16U];
    uint8_t data_structure_nesting_field[16U];
    uint8_t protocol_version_field[16U];
    uint8_t parameter_cbb_field[16U];
    uint8_t services_supported_field[32U];
    uint8_t detail_fields[128U];
    uint8_t detail_wrapper[160U];
    size_t max_pdu_size_length = 0U;
    size_t max_serv_out_calling_length = 0U;
    size_t max_serv_out_called_length = 0U;
    size_t data_structure_nesting_length = 0U;
    size_t protocol_version_length = 0U;
    size_t parameter_cbb_length = 0U;
    size_t services_supported_length = 0U;
    size_t detail_fields_length = 0U;
    size_t detail_wrapper_length = 0U;
    const uint8_t max_pdu_size_value[] = { 0x00U, 0xFDU, 0xE8U };
    const uint8_t max_serv_outstanding_value[] = { 0x05U };
    const uint8_t data_structure_nesting_value[] = { 0x0AU };
    const uint8_t protocol_version_value[] = { 0x01U };
    const uint8_t parameter_cbb_value[] = { 0x05U, 0xF1U, 0x00U };
    const uint8_t services_supported_value[] = { 0x03U, 0xEEU, 0x1CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x18U };

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Initiate response detail requires buffer and encoded_length.");
        return 0;
    }

    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            0U,
            max_pdu_size_value,
            sizeof(max_pdu_size_value),
            max_pdu_size_field,
            sizeof(max_pdu_size_field),
            &max_pdu_size_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            1U,
            max_serv_outstanding_value,
            sizeof(max_serv_outstanding_value),
            max_serv_out_calling_field,
            sizeof(max_serv_out_calling_field),
            &max_serv_out_calling_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            2U,
            max_serv_outstanding_value,
            sizeof(max_serv_outstanding_value),
            max_serv_out_called_field,
            sizeof(max_serv_out_called_field),
            &max_serv_out_called_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            3U,
            data_structure_nesting_value,
            sizeof(data_structure_nesting_value),
            data_structure_nesting_field,
            sizeof(data_structure_nesting_field),
            &data_structure_nesting_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            0U,
            protocol_version_value,
            sizeof(protocol_version_value),
            protocol_version_field,
            sizeof(protocol_version_field),
            &protocol_version_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            1U,
            parameter_cbb_value,
            sizeof(parameter_cbb_value),
            parameter_cbb_field,
            sizeof(parameter_cbb_field),
            &parameter_cbb_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            2U,
            services_supported_value,
            sizeof(services_supported_value),
            services_supported_field,
            sizeof(services_supported_field),
            &services_supported_length,
            diagnostic)) {
        return 0;
    }
    if (protocol_version_length + parameter_cbb_length + services_supported_length > sizeof(detail_fields)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Initiate response detail payload is too large.");
        return 0;
    }
    memcpy(detail_fields, protocol_version_field, protocol_version_length);
    memcpy(detail_fields + protocol_version_length, parameter_cbb_field, parameter_cbb_length);
    memcpy(detail_fields + protocol_version_length + parameter_cbb_length, services_supported_field, services_supported_length);
    detail_fields_length = protocol_version_length + parameter_cbb_length + services_supported_length;

    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            4U,
            detail_fields,
            detail_fields_length,
            detail_wrapper,
            sizeof(detail_wrapper),
            &detail_wrapper_length,
            diagnostic)) {
        return 0;
    }
    if (max_pdu_size_length + max_serv_out_calling_length + max_serv_out_called_length + data_structure_nesting_length + detail_wrapper_length > buffer_length) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Initiate response detail buffer is too small.");
        return 0;
    }
    {
        size_t offset = 0U;
        if (!wire_builder_append_bytes(buffer, buffer_length, &offset, max_pdu_size_field, max_pdu_size_length, diagnostic)) {
            return 0;
        }
        if (!wire_builder_append_bytes(buffer, buffer_length, &offset, max_serv_out_calling_field, max_serv_out_calling_length, diagnostic)) {
            return 0;
        }
        if (!wire_builder_append_bytes(buffer, buffer_length, &offset, max_serv_out_called_field, max_serv_out_called_length, diagnostic)) {
            return 0;
        }
        if (!wire_builder_append_bytes(buffer, buffer_length, &offset, data_structure_nesting_field, data_structure_nesting_length, diagnostic)) {
            return 0;
        }
        if (!wire_builder_append_bytes(buffer, buffer_length, &offset, detail_wrapper, detail_wrapper_length, diagnostic)) {
            return 0;
        }
        *encoded_length = offset;
    }
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

static int wire_builder_build_initiate_response_pdu(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t detail_bytes[192U];
    size_t detail_length = 0U;
    UnitLabMmsPdu pdu;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response PDU requires buffer and encoded_length.");
        return 0;
    }
    if (!wire_builder_build_initiate_response_detail(detail_bytes, sizeof(detail_bytes), &detail_length, diagnostic)) {
        return 0;
    }
    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_INITIATE_RESPONSE;
    pdu.pdu_bytes = detail_bytes;
    pdu.pdu_length = detail_length;
    return unitlab_mms_pdu_encode(&pdu, buffer, buffer_length, encoded_length, diagnostic);
}

static int wire_builder_build_association_response_acse(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t initiate_response_bytes[256U];
    uint8_t external_choice_bytes[288U];
    uint8_t external_bytes[320U];
    uint8_t user_information_bytes[352U];
    uint8_t a1_oid_bytes[32U];
    uint8_t a2_integer_bytes[16U];
    uint8_t a3_inner_bytes[16U];
    uint8_t a3_bytes[32U];
    uint8_t app_inner_bytes[352U];
    uint8_t app_wrapper_bytes[384U];
    uint8_t a0_wrapper_bytes[416U];
    uint8_t outer_sequence_bytes[448U];
    uint8_t integer_one_bytes[16U];
    size_t initiate_response_length = 0U;
    size_t external_choice_length = 0U;
    size_t external_length = 0U;
    size_t user_information_length = 0U;
    size_t a1_oid_length = 0U;
    size_t a2_integer_length = 0U;
    size_t a3_inner_length = 0U;
    size_t a3_length = 0U;
    size_t app_inner_length = 0U;
    size_t app_wrapper_length = 0U;
    size_t a0_wrapper_length = 0U;
    size_t outer_sequence_length = 0U;
    size_t integer_one_length = 0U;
    UnitLabMmsAcseApdu acse_apdu;
    const uint8_t integer_one_value[] = { 0x01U };
    const uint8_t oid_value[] = { 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U };
    const uint8_t integer_zero_value[] = { 0x00U };
    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response ACSE requires buffer and encoded_length.");
        return 0;
    }

    if (!wire_builder_build_initiate_response_pdu(
            initiate_response_bytes,
            sizeof(initiate_response_bytes),
            &initiate_response_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            2U,
            integer_one_value,
            sizeof(integer_one_value),
            integer_one_bytes,
            sizeof(integer_one_bytes),
            &integer_one_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            initiate_response_bytes,
            initiate_response_length,
            external_choice_bytes,
            sizeof(external_choice_bytes),
            &external_choice_length,
            diagnostic)) {
        return 0;
    }
    if (integer_one_length + external_choice_length > sizeof(external_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response ACSE external wrapper is too large.");
        return 0;
    }
    memcpy(external_bytes, integer_one_bytes, integer_one_length);
    memcpy(external_bytes + integer_one_length, external_choice_bytes, external_choice_length);
    external_length = integer_one_length + external_choice_length;
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            8U,
            external_bytes,
            external_length,
            user_information_bytes,
            sizeof(user_information_bytes),
            &user_information_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            6U,
            oid_value,
            sizeof(oid_value),
            a1_oid_bytes,
            sizeof(a1_oid_bytes),
            &a1_oid_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            a1_oid_bytes,
            a1_oid_length,
            a2_integer_bytes,
            sizeof(a2_integer_bytes),
            &a2_integer_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            2U,
            integer_zero_value,
            sizeof(integer_zero_value),
            a3_inner_bytes,
            sizeof(a3_inner_bytes),
            &a3_inner_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            a3_inner_bytes,
            a3_inner_length,
            a3_bytes,
            sizeof(a3_bytes),
            &a3_length,
            diagnostic)) {
        return 0;
    }
    if (a1_oid_length + a2_integer_length + a3_length + user_information_length > sizeof(app_inner_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response ACSE application payload is too large.");
        return 0;
    }
    memcpy(app_inner_bytes, a1_oid_bytes, a1_oid_length);
    memcpy(app_inner_bytes + a1_oid_length, a2_integer_bytes, a2_integer_length);
    memcpy(app_inner_bytes + a1_oid_length + a2_integer_length, a3_bytes, a3_length);
    memcpy(app_inner_bytes + a1_oid_length + a2_integer_length + a3_length, user_information_bytes, user_information_length);
    app_inner_length = a1_oid_length + a2_integer_length + a3_length + user_information_length;
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_APPLICATION,
            1,
            1U,
            app_inner_bytes,
            app_inner_length,
            app_wrapper_bytes,
            sizeof(app_wrapper_bytes),
            &app_wrapper_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            app_wrapper_bytes,
            app_wrapper_length,
            a0_wrapper_bytes,
            sizeof(a0_wrapper_bytes),
            &a0_wrapper_length,
            diagnostic)) {
        return 0;
    }
    if (integer_one_length + a0_wrapper_length > sizeof(outer_sequence_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response ACSE outer sequence is too large.");
        return 0;
    }
    memcpy(outer_sequence_bytes, integer_one_bytes, integer_one_length);
    memcpy(outer_sequence_bytes + integer_one_length, a0_wrapper_bytes, a0_wrapper_length);
    outer_sequence_length = integer_one_length + a0_wrapper_length;

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARE;
    acse_apdu.apdu_bytes = outer_sequence_bytes;
    acse_apdu.apdu_length = outer_sequence_length;
    return unitlab_mms_acse_encode(&acse_apdu, buffer, buffer_length, encoded_length, diagnostic);
}

static int wire_builder_build_association_response_session(
    const uint8_t* presentation_bytes,
    size_t presentation_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    size_t offset = 0U;
    uint8_t session_accept_item[] = { 0x13U, 0x01U, 0x00U, 0x16U, 0x01U, 0x02U };
    uint8_t session_requirement[] = { 0x00U, 0x02U };
    uint8_t session_selector[] = { 0x00U, 0x01U };

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response session requires buffer and encoded_length.");
        return 0;
    }
    if (presentation_length != 0U && presentation_bytes == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response session requires presentation bytes when length is non-zero.");
        return 0;
    }
    if (buffer_length < 2U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response session buffer is too small.");
        return 0;
    }

    buffer[offset++] = 0x0EU;
    buffer[offset++] = 0x00U;
    if (!wire_builder_append_short_tlv(0x05U, session_accept_item, sizeof(session_accept_item), buffer, buffer_length, &offset, diagnostic)) {
        return 0;
    }
    if (!wire_builder_append_short_tlv(0x14U, session_requirement, sizeof(session_requirement), buffer, buffer_length, &offset, diagnostic)) {
        return 0;
    }
    if (!wire_builder_append_short_tlv(0x34U, session_selector, sizeof(session_selector), buffer, buffer_length, &offset, diagnostic)) {
        return 0;
    }
    if (!wire_builder_append_short_tlv(0xC1U, presentation_bytes, presentation_length, buffer, buffer_length, &offset, diagnostic)) {
        return 0;
    }
    if (offset < 2U || offset - 2U > 0xFFU) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response session length is too large.");
        return 0;
    }
    buffer[1U] = (uint8_t)(offset - 2U);
    *encoded_length = offset;
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

static int wire_builder_build_association_response_frame_structured(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t acse_bytes[512U];
    uint8_t presentation_a0_bytes[16U];
    uint8_t presentation_a2_inner_bytes[256U];
    uint8_t presentation_a2_bytes[256U];
    uint8_t presentation_set_bytes[512U];
    uint8_t session_bytes[512U];
    uint8_t presentation_content_selector_a5[] = { 0x30U, 0x07U, 0x80U, 0x01U, 0x00U, 0x81U, 0x02U, 0x51U, 0x01U, 0x30U, 0x07U, 0x80U, 0x01U, 0x00U, 0x81U, 0x02U, 0x51U, 0x01U };
    UnitLabMmsTransportFrame transport_frame;
    size_t acse_length = 0U;
    size_t presentation_a0_length = 0U;
    size_t presentation_a2_inner_length = 0U;
    size_t presentation_a2_length = 0U;
    size_t presentation_set_length = 0U;
    size_t session_length = 0U;
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

    if (!wire_builder_build_association_response_acse(acse_bytes, sizeof(acse_bytes), &acse_length, diagnostic)) {
        return 0;
    }
    {
        uint8_t presentation_a0_value[] = { 0x80U, 0x01U, 0x01U };
        if (!wire_builder_encode_nested_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                1,
                0U,
                presentation_a0_value,
                sizeof(presentation_a0_value),
                presentation_a0_bytes,
                sizeof(presentation_a0_bytes),
                &presentation_a0_length,
                diagnostic)) {
            return 0;
        }
    }
    {
        uint8_t presentation_context_selector_bytes[] = { 0x83U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U };
        if (!wire_builder_append_bytes(presentation_a2_inner_bytes, sizeof(presentation_a2_inner_bytes), &presentation_a2_inner_length, presentation_context_selector_bytes, sizeof(presentation_context_selector_bytes), diagnostic)) {
            return 0;
        }
    }
    if (!wire_builder_append_bytes(presentation_a2_inner_bytes, sizeof(presentation_a2_inner_bytes), &presentation_a2_inner_length, presentation_content_selector_a5, sizeof(presentation_content_selector_a5), diagnostic)) {
        return 0;
    }
    if (!wire_builder_append_bytes(presentation_a2_inner_bytes, sizeof(presentation_a2_inner_bytes), &presentation_a2_inner_length, acse_bytes, acse_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            2U,
            presentation_a2_inner_bytes,
            presentation_a2_inner_length,
            presentation_a2_bytes,
            sizeof(presentation_a2_bytes),
            &presentation_a2_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_append_bytes(presentation_set_bytes, sizeof(presentation_set_bytes), &presentation_set_length, presentation_a0_bytes, presentation_a0_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_append_bytes(presentation_set_bytes, sizeof(presentation_set_bytes), &presentation_set_length, presentation_a2_bytes, presentation_a2_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_build_association_response_session(presentation_set_bytes, presentation_set_length, session_bytes, sizeof(session_bytes), &session_length, diagnostic)) {
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
