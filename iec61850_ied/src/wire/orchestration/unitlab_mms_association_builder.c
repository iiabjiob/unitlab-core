#include "unitlab_mms_wire_builder_internal.h"

#include <string.h>

#include "wire/acse/unitlab_mms_acse.h"
#include "wire/presentation/unitlab_mms_presentation.h"
#include "wire/session/unitlab_mms_session_spdu.h"
#include "wire/transport/unitlab_mms_transport_frame.h"

void unitlab_mms_initiate_response_profile_init(UnitLabMmsInitiateResponseProfile* profile)
{
    if (profile == NULL) {
        return;
    }
    profile->local_detail_called = 65000U;
    profile->max_serv_outstanding_calling = 5U;
    profile->max_serv_outstanding_called = 5U;
    profile->data_structure_nesting_level = 5U;
    profile->negotiated_version_number = 1U;
    profile->parameter_cbb[0] = 0x05U;
    profile->parameter_cbb[1] = 0xF1U;
    profile->parameter_cbb[2] = 0x00U;
    profile->parameter_cbb_length = UNITLAB_MMS_INITIATE_RESPONSE_PROFILE_PARAMETER_CBB_LENGTH;
    profile->services_supported_called[0] = 0x03U;
    profile->services_supported_called[1] = 0xEEU;
    profile->services_supported_called[2] = 0x1CU;
    profile->services_supported_called[3] = 0x00U;
    profile->services_supported_called[4] = 0x00U;
    profile->services_supported_called[5] = 0x00U;
    profile->services_supported_called[6] = 0x02U;
    profile->services_supported_called[7] = 0x00U;
    profile->services_supported_called[8] = 0x00U;
    profile->services_supported_called[9] = 0x40U;
    profile->services_supported_called[10] = 0xEDU;
    profile->services_supported_called[11] = 0x18U;
    profile->services_supported_called_length = UNITLAB_MMS_INITIATE_RESPONSE_PROFILE_SERVICES_SUPPORTED_LENGTH;
}

void unitlab_mms_initiate_response_profile_apply_model_plan(UnitLabMmsInitiateResponseProfile* profile, const UnitLabIedModelPlan* plan)
{
    uint32_t local_detail_called = 8000U;
    uint32_t max_serv_outstanding_calling = 1U;
    uint32_t max_serv_outstanding_called = 1U;

    if (profile == NULL || plan == NULL) {
        return;
    }

    if (plan->logical_device_count > 1U) {
        local_detail_called += 1024U;
    }
    if (plan->data_set_count > 1U) {
        local_detail_called += 512U;
    }
    if (plan->report_count > 1U) {
        local_detail_called += 512U;
    }
    if (plan->signal_count > 8U) {
        local_detail_called += 256U;
    }
    if (plan->signal_count > 16U) {
        local_detail_called += 256U;
    }
    profile->local_detail_called = wire_builder_clamp_uint32(local_detail_called, 8000U, 65000U);

    max_serv_outstanding_calling += (plan->report_count > 1U) ? 1U : 0U;
    max_serv_outstanding_calling += (plan->report_count > 4U) ? 1U : 0U;
    profile->max_serv_outstanding_calling = wire_builder_clamp_uint32(max_serv_outstanding_calling, 1U, 5U);

    max_serv_outstanding_called += (plan->data_set_count > 2U) ? 1U : 0U;
    max_serv_outstanding_called += (plan->signal_count > 16U) ? 1U : 0U;
    profile->max_serv_outstanding_called = wire_builder_clamp_uint32(max_serv_outstanding_called, 1U, 5U);

    profile->data_structure_nesting_level = wire_builder_calculate_model_nesting_level(plan);
    profile->negotiated_version_number = 1U;
}

static int wire_builder_build_initiate_response_detail(const UnitLabMmsInitiateResponseProfile* profile, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
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
    uint8_t max_pdu_size_value[5U];
    size_t max_pdu_size_length = 0U;
    size_t max_serv_out_calling_length = 0U;
    size_t max_serv_out_called_length = 0U;
    size_t data_structure_nesting_length = 0U;
    size_t protocol_version_length = 0U;
    size_t parameter_cbb_length = 0U;
    size_t services_supported_length = 0U;
    size_t detail_fields_length = 0U;
    size_t detail_wrapper_length = 0U;


    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Initiate response detail requires buffer and encoded_length.");
        return 0;
    }

    if (!wire_builder_encode_unsigned_integer(profile->local_detail_called, max_pdu_size_value, sizeof(max_pdu_size_value), &max_pdu_size_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            0U,
            max_pdu_size_value,
            max_pdu_size_length,
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
            profile->max_serv_outstanding_calling <= 0xFFU ? (const uint8_t[]){ (uint8_t)profile->max_serv_outstanding_calling } : (const uint8_t[]){ 0x05U },
            1U,
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
            profile->max_serv_outstanding_calling <= 0xFFU ? (const uint8_t[]){ (uint8_t)profile->max_serv_outstanding_calling } : (const uint8_t[]){ 0x05U },
            1U,
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
            profile->data_structure_nesting_level <= 0xFFU ? (const uint8_t[]){ (uint8_t)profile->data_structure_nesting_level } : (const uint8_t[]){ 0x0AU },
            1U,
            data_structure_nesting_field,
            sizeof(data_structure_nesting_field),
            &data_structure_nesting_length,
            diagnostic)) {
        return 0;
    }
    {
        uint8_t protocol_version_value[] = { profile->negotiated_version_number };
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
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            1U,
            profile->parameter_cbb,
            profile->parameter_cbb_length,
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
            profile->services_supported_called,
            profile->services_supported_called_length,
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

static int wire_builder_build_initiate_response_pdu(const UnitLabMmsInitiateResponseProfile* profile, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
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
    if (profile == NULL) {
        static UnitLabMmsInitiateResponseProfile default_profile;
        static int profile_initialized = 0;
        if (!profile_initialized) {
            unitlab_mms_initiate_response_profile_init(&default_profile);
            profile_initialized = 1;
        }
        profile = &default_profile;
    }
    if (!wire_builder_build_initiate_response_detail(profile, detail_bytes, sizeof(detail_bytes), &detail_length, diagnostic)) {
        return 0;
    }
    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_INITIATE_RESPONSE;
    pdu.pdu_bytes = detail_bytes;
    pdu.pdu_length = detail_length;
    return unitlab_mms_pdu_encode(&pdu, buffer, buffer_length, encoded_length, diagnostic);
}

static int wire_builder_build_association_accept_acse(
    const UnitLabMmsInitiateResponseProfile* profile,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t initiate_response_bytes[256U];
    size_t initiate_response_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept ACSE requires buffer and encoded_length.");
        return 0;
    }

    if (!wire_builder_build_initiate_response_pdu(
            profile,
            initiate_response_bytes,
            sizeof(initiate_response_bytes),
            &initiate_response_length,
            diagnostic)) {
        return 0;
    }

    return unitlab_mms_acse_build_association_accept_frame(
        initiate_response_bytes,
        initiate_response_length,
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

static int wire_builder_build_presentation_context_result_item(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t result_bytes[16U];
    uint8_t transfer_syntax_bytes[16U];
    uint8_t sequence_bytes[32U];
    size_t result_length = 0U;
    size_t transfer_syntax_length = 0U;
    size_t sequence_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Presentation context result item requires buffer and encoded_length.");
        return 0;
    }

    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            0U,
            (const uint8_t[]){ 0x00U },
            1U,
            result_bytes,
            sizeof(result_bytes),
            &result_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            1U,
            (const uint8_t[]){ 0x51U, 0x01U },
            2U,
            transfer_syntax_bytes,
            sizeof(transfer_syntax_bytes),
            &transfer_syntax_length,
            diagnostic)) {
        return 0;
    }
    if (result_length + transfer_syntax_length > sizeof(sequence_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Presentation context result item buffer is too small.");
        return 0;
    }
    memcpy(sequence_bytes, result_bytes, result_length);
    memcpy(sequence_bytes + result_length, transfer_syntax_bytes, transfer_syntax_length);
    sequence_length = result_length + transfer_syntax_length;
    return wire_builder_encode_nested_element(
        UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
        1,
        16U,
        sequence_bytes,
        sequence_length,
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

static int wire_builder_build_presentation_context_result_list(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t item_bytes[64U];
    uint8_t list_content[96U];
    size_t item_length = 0U;
    size_t list_content_length = 0U;
    size_t second_item_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Presentation context result list requires buffer and encoded_length.");
        return 0;
    }

    if (!wire_builder_build_presentation_context_result_item(item_bytes, sizeof(item_bytes), &item_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_build_presentation_context_result_item(item_bytes + item_length, sizeof(item_bytes) - item_length, &second_item_length, diagnostic)) {
        return 0;
    }
    if (item_length + second_item_length > sizeof(list_content)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Presentation context result list buffer is too small.");
        return 0;
    }
    memcpy(list_content, item_bytes, item_length);
    memcpy(list_content + item_length, item_bytes + item_length, second_item_length);
    list_content_length = item_length + second_item_length;
    return wire_builder_encode_nested_element(
        UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
        1,
        5U,
        list_content,
        list_content_length,
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

static int wire_builder_build_association_accept_presentation(
    const uint8_t* acse_bytes,
    size_t acse_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsPresentationApdu presentation_apdu;
    uint8_t user_data_bytes[512U];
    uint8_t normal_mode_value_bytes[16U];
    uint8_t normal_mode_selector_bytes[16U];
    uint8_t responding_selector_bytes[16U];
    uint8_t result_list_bytes[96U];
    uint8_t normal_mode_parameters_content[768U];
    uint8_t normal_mode_parameters_bytes[896U];
    uint8_t presentation_bytes[1024U];
    size_t user_data_length = 0U;
    size_t normal_mode_value_length = 0U;
    size_t normal_mode_selector_length = 0U;
    size_t responding_selector_length = 0U;
    size_t result_list_length = 0U;
    size_t normal_mode_parameters_content_length = 0U;
    size_t normal_mode_parameters_length = 0U;
    size_t presentation_length = 0U;
    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept presentation requires buffer and encoded_length.");
        return 0;
    }
    if (acse_length != 0U && acse_bytes == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept presentation requires ACSE bytes when length is non-zero.");
        return 0;
    }

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    presentation_apdu.context_identifier = 1U;
    presentation_apdu.payload_bytes = acse_bytes;
    presentation_apdu.payload_length = acse_length;
    if (!unitlab_mms_presentation_encode(&presentation_apdu, user_data_bytes, sizeof(user_data_bytes), &user_data_length, diagnostic)) {
        return 0;
    }

    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            0U,
            (const uint8_t[]){ 0x01U },
            1U,
            normal_mode_value_bytes,
            sizeof(normal_mode_value_bytes),
            &normal_mode_value_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            normal_mode_value_bytes,
            normal_mode_value_length,
            normal_mode_selector_bytes,
            sizeof(normal_mode_selector_bytes),
            &normal_mode_selector_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            3U,
            (const uint8_t[]){ 0x00U, 0x00U, 0x00U, 0x01U },
            4U,
            responding_selector_bytes,
            sizeof(responding_selector_bytes),
            &responding_selector_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_build_presentation_context_result_list(result_list_bytes, sizeof(result_list_bytes), &result_list_length, diagnostic)) {
        return 0;
    }
    if (responding_selector_length + result_list_length + user_data_length > sizeof(normal_mode_parameters_content)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept presentation buffer is too small.");
        return 0;
    }
    memcpy(normal_mode_parameters_content, responding_selector_bytes, responding_selector_length);
    memcpy(normal_mode_parameters_content + responding_selector_length, result_list_bytes, result_list_length);
    memcpy(normal_mode_parameters_content + responding_selector_length + result_list_length, user_data_bytes, user_data_length);
    normal_mode_parameters_content_length = responding_selector_length + result_list_length + user_data_length;

    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            2U,
            normal_mode_parameters_content,
            normal_mode_parameters_content_length,
            normal_mode_parameters_bytes,
            sizeof(normal_mode_parameters_bytes),
            &normal_mode_parameters_length,
            diagnostic)) {
        return 0;
    }

    if (normal_mode_selector_length + normal_mode_parameters_length > sizeof(presentation_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept presentation buffer is too small.");
        return 0;
    }
    memcpy(presentation_bytes, normal_mode_selector_bytes, normal_mode_selector_length);
    memcpy(presentation_bytes + normal_mode_selector_length, normal_mode_parameters_bytes, normal_mode_parameters_length);
    presentation_length = normal_mode_selector_length + normal_mode_parameters_length;

    return wire_builder_encode_nested_element(
        UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
        1,
        17U,
        presentation_bytes,
        presentation_length,
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

static int wire_builder_build_association_accept_session(
    const uint8_t* presentation_bytes,
    size_t presentation_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsSessionSpdu session_spdu;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept session requires buffer and encoded_length.");
        return 0;
    }
    if (presentation_length != 0U && presentation_bytes == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept session requires presentation bytes when length is non-zero.");
        return 0;
    }

    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_ACCEPT;
    session_spdu.spdu_bytes = presentation_bytes;
    session_spdu.spdu_length = presentation_length;
    return unitlab_mms_session_spdu_encode(&session_spdu, buffer, buffer_length, encoded_length, diagnostic);
}

static int wire_builder_build_association_accept_transport(
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
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept transport requires buffer and encoded_length.");
        return 0;
    }
    if (session_length != 0U && session_bytes == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept transport requires session bytes when length is non-zero.");
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

static int wire_builder_build_association_accept_frame_structured(
    const UnitLabMmsInitiateResponseProfile* profile,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t acse_bytes[512U];
    uint8_t presentation_bytes[512U];
    uint8_t session_bytes[512U];
    size_t acse_length = 0U;
    size_t presentation_length = 0U;
    size_t session_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept frame requires buffer and encoded_length.");
        return 0;
    }
    if (buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept frame buffer must be non-zero.");
        return 0;
    }

    if (profile == NULL) {
        static UnitLabMmsInitiateResponseProfile default_profile;
        static int profile_initialized = 0;
        if (!profile_initialized) {
            unitlab_mms_initiate_response_profile_init(&default_profile);
            profile_initialized = 1;
        }
        profile = &default_profile;
    }
    if (!wire_builder_build_association_accept_acse(profile, acse_bytes, sizeof(acse_bytes), &acse_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_build_association_accept_presentation(acse_bytes, acse_length, presentation_bytes, sizeof(presentation_bytes), &presentation_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_build_association_accept_session(presentation_bytes, presentation_length, session_bytes, sizeof(session_bytes), &session_length, diagnostic)) {
        return 0;
    }
    if (!wire_builder_build_association_accept_transport(session_bytes, session_length, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}

int unitlab_mms_build_association_response_frame_with_profile(
    const UnitLabMmsInitiateResponseProfile* profile,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    return wire_builder_build_association_accept_frame_structured(profile, buffer, buffer_length, encoded_length, diagnostic);
}

int unitlab_mms_build_association_response_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    return wire_builder_build_association_accept_frame_structured(NULL, buffer, buffer_length, encoded_length, diagnostic);
}

