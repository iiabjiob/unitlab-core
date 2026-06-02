#include "unitlab_mms_wire_builder.h"

#include <stdio.h>
#include <string.h>

#include "wire/acse/unitlab_mms_acse.h"
#include "wire/presentation/unitlab_mms_presentation.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/orchestration/unitlab_mms_association_frame.h"
#include "model/model_plan.h"

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

static int wire_builder_encode_unsigned_integer(uint32_t value, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t bytes[5U];
    size_t length = 0U;
    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    do {
        bytes[sizeof(bytes) - 1U - length] = (uint8_t)(value & 0xFFU);
        length++;
        value >>= 8U;
    } while (value != 0U && length < sizeof(bytes));
    if (value != 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding is too large.");
        return 0;
    }
    if (bytes[sizeof(bytes) - length] & 0x80U) {
        if (length == sizeof(bytes)) {
            wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding is too large.");
            return 0;
        }
        bytes[sizeof(bytes) - length - 1U] = 0x00U;
        length++;
    }
    if (buffer_length < length) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding buffer is too small.");
        return 0;
    }
    memcpy(buffer, &bytes[sizeof(bytes) - length], length);
    *encoded_length = length;
    return 1;
}

static size_t wire_builder_count_path_depth(const char* reference)
{
    size_t depth = 0U;
    int in_segment = 0;

    if (reference == NULL || reference[0] == '\0') {
        return 0U;
    }

    for (const char* cursor = reference; *cursor != '\0'; cursor++) {
        if (*cursor == '.') {
            if (in_segment) {
                depth++;
                in_segment = 0;
            }
            continue;
        }
        if (!in_segment) {
            in_segment = 1;
        }
    }
    if (in_segment) {
        depth++;
    }
    return depth;
}

static size_t wire_builder_calculate_model_nesting_level(const UnitLabIedModelPlan* plan)
{
    size_t max_depth = 0U;

    if (plan == NULL || plan->signal_count == 0U || plan->signals == NULL) {
        return 5U;
    }
    for (size_t index = 0U; index < plan->signal_count; index++) {
        size_t depth = wire_builder_count_path_depth(plan->signals[index].object_reference);
        if (depth > max_depth) {
            max_depth = depth;
        }
    }
    if (max_depth == 0U) {
        return 5U;
    }
    max_depth += 2U;
    return max_depth < 5U ? 5U : max_depth;
}

static uint32_t wire_builder_clamp_uint32(uint32_t value, uint32_t minimum, uint32_t maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
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
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsTransportFrame transport_frame;
    uint8_t presentation_bytes[512U];
    uint8_t session_bytes[512U];
    size_t payload_length = 0U;
    size_t presentation_length = 0U;
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

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    if (pdu->kind == UNITLAB_MMS_PDU_UNCONFIRMED) {
        if (!unitlab_mms_pdu_encode(pdu, scratch, scratch_length, &payload_length, diagnostic)) {
            return 0;
        }
        presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
        presentation_apdu.payload_bytes = scratch;
        presentation_apdu.payload_length = payload_length;
    } else {
        presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
        presentation_apdu.payload_bytes = pdu->pdu_bytes;
        presentation_apdu.payload_length = pdu->pdu_length;
    }
    if (!unitlab_mms_presentation_encode(&presentation_apdu, presentation_bytes, sizeof(presentation_bytes), &presentation_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_spdu.spdu_bytes = presentation_bytes;
    session_spdu.spdu_length = presentation_length;
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


static int wire_builder_build_association_accept_frame_structured(
    const UnitLabMmsInitiateResponseProfile* profile,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

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

void unitlab_mms_initiate_response_profile_init(UnitLabMmsInitiateResponseProfile* profile)
{
    if (profile == NULL) {
        return;
    }
    profile->local_detail_called = 8000U;
    profile->max_serv_outstanding_calling = 1U;
    profile->max_serv_outstanding_called = 1U;
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
    profile->services_supported_called[6] = 0x00U;
    profile->services_supported_called[7] = 0x00U;
    profile->services_supported_called[8] = 0x00U;
    profile->services_supported_called[9] = 0x00U;
    profile->services_supported_called[10] = 0x01U;
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
            9U,
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

static int wire_builder_build_association_accept_presentation(
    const uint8_t* acse_bytes,
    size_t acse_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsPresentationApdu presentation_apdu;

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
    presentation_apdu.payload_bytes = acse_bytes;
    presentation_apdu.payload_length = acse_length;
    return unitlab_mms_presentation_encode(&presentation_apdu, buffer, buffer_length, encoded_length, diagnostic);
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
