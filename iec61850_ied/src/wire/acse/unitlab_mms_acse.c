#include "unitlab_mms_acse.h"

#include <string.h>

static void acse_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

static int acse_encode_nested_element(
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

static int acse_kind_to_tag(UnitLabMmsAcseApduKind kind, UnitLabMmsBerTag* tag)
{
    if (tag == NULL) {
        return 0;
    }
    unitlab_mms_ber_tag_init(tag);
    tag->tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
    tag->constructed = 1;
    switch (kind) {
        case UNITLAB_MMS_ACSE_APDU_AARQ:
            tag->tag_number = 0U;
            return 1;
        case UNITLAB_MMS_ACSE_APDU_AARE:
            tag->tag_number = 1U;
            return 1;
        case UNITLAB_MMS_ACSE_APDU_RLRQ:
            tag->tag_number = 2U;
            return 1;
        case UNITLAB_MMS_ACSE_APDU_RLRE:
            tag->tag_number = 3U;
            return 1;
        case UNITLAB_MMS_ACSE_APDU_ABRT:
            tag->tag_number = 4U;
            return 1;
        default:
            return 0;
    }
}

static int acse_tag_to_kind(const UnitLabMmsBerTag* tag, UnitLabMmsAcseApduKind* kind)
{
    if (tag == NULL || kind == NULL) {
        return 0;
    }
    if (tag->tag_class != UNITLAB_MMS_BER_TAG_CLASS_APPLICATION || !tag->constructed) {
        return 0;
    }
    switch (tag->tag_number) {
        case 0U:
            *kind = UNITLAB_MMS_ACSE_APDU_AARQ;
            return 1;
        case 1U:
            *kind = UNITLAB_MMS_ACSE_APDU_AARE;
            return 1;
        case 2U:
            *kind = UNITLAB_MMS_ACSE_APDU_RLRQ;
            return 1;
        case 3U:
            *kind = UNITLAB_MMS_ACSE_APDU_RLRE;
            return 1;
        case 4U:
            *kind = UNITLAB_MMS_ACSE_APDU_ABRT;
            return 1;
        default:
            return 0;
    }
}

static int acse_parse_raw_fields(const uint8_t* buffer, size_t buffer_length, UnitLabMmsAcseApdu* apdu, UnitLabMmsDiagnostic* diagnostic)
{
    size_t offset = 0U;

    if (apdu == NULL) {
        return 0;
    }
    apdu->field_count = 0U;
    while (offset < buffer_length) {
        size_t consumed_length = 0U;
        if (apdu->field_count >= (sizeof(apdu->fields) / sizeof(apdu->fields[0]))) {
            acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "ACSE APDU contains too many raw fields.");
            return 0;
        }
        unitlab_mms_ber_element_init(&apdu->fields[apdu->field_count]);
        if (!unitlab_mms_ber_read(&apdu->fields[apdu->field_count], &buffer[offset], buffer_length - offset, &consumed_length, diagnostic)) {
            return 0;
        }
        if (consumed_length == 0U) {
            acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "ACSE APDU contains an empty raw field.");
            return 0;
        }
        if (consumed_length > buffer_length - offset) {
            acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "ACSE raw field length exceeds the remaining buffer.");
            return 0;
        }
        offset += consumed_length;
        apdu->field_count++;
    }
    return 1;
}

void unitlab_mms_acse_apdu_init(UnitLabMmsAcseApdu* apdu)
{
    if (apdu == NULL) {
        return;
    }
    memset(apdu, 0, sizeof(*apdu));
    apdu->kind = UNITLAB_MMS_ACSE_APDU_NONE;
}

int unitlab_mms_acse_encode(const UnitLabMmsAcseApdu* apdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;
    UnitLabMmsBerTag tag;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (apdu == NULL || buffer == NULL || encoded_length == NULL) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "ACSE encode requires apdu, buffer, and encoded_length.");
        return 0;
    }
    if (apdu->apdu_length != 0U && apdu->apdu_bytes == NULL) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "ACSE APDU bytes are required when APDU length is non-zero.");
        return 0;
    }
    if (!acse_kind_to_tag(apdu->kind, &tag)) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported ACSE APDU kind.");
        return 0;
    }
    unitlab_mms_ber_element_init(&element);
    element.tag = tag;
    element.value_bytes = apdu->apdu_bytes;
    element.value_length = apdu->apdu_length;
    if (!unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_acse_build_association_accept_frame(const uint8_t* initiate_response_bytes, size_t initiate_response_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t application_context_name_inner_bytes[32U];
    uint8_t application_context_name_bytes[32U];
    uint8_t result_inner_bytes[16U];
    uint8_t result_bytes[16U];
    uint8_t result_source_inner_bytes[16U];
    uint8_t result_source_choice_bytes[16U];
    uint8_t result_source_bytes[32U];
    uint8_t initiate_response_wrapper_bytes[320U];
    uint8_t indirect_reference_bytes[16U];
    uint8_t association_data_content_bytes[352U];
    uint8_t association_data_bytes[384U];
    uint8_t user_information_bytes[448U];
    uint8_t sequence_bytes[512U];
    uint8_t aare_fields_bytes[512U];
    size_t application_context_name_inner_length = 0U;
    size_t application_context_name_length = 0U;
    size_t result_inner_length = 0U;
    size_t result_length = 0U;
    size_t result_source_inner_length = 0U;
    size_t result_source_choice_length = 0U;
    size_t result_source_length = 0U;
    size_t initiate_response_wrapper_length = 0U;
    size_t indirect_reference_length = 0U;
    size_t association_data_content_length = 0U;
    size_t association_data_length = 0U;
    size_t user_information_length = 0U;
    size_t sequence_length = 0U;
    size_t aare_fields_length = 0U;
    UnitLabMmsAcseApdu acse_apdu;
    const uint8_t oid_value[] = { 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U };
    const uint8_t integer_zero_value[] = { 0x00U };

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept ACSE requires buffer and encoded_length.");
        return 0;
    }
    if (initiate_response_length != 0U && initiate_response_bytes == NULL) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept ACSE requires initiate response bytes when length is non-zero.");
        return 0;
    }

    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            6U,
            oid_value,
            sizeof(oid_value),
            application_context_name_inner_bytes,
            sizeof(application_context_name_inner_bytes),
            &application_context_name_inner_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            application_context_name_inner_bytes,
            application_context_name_inner_length,
            application_context_name_bytes,
            sizeof(application_context_name_bytes),
            &application_context_name_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            2U,
            integer_zero_value,
            sizeof(integer_zero_value),
            result_inner_bytes,
            sizeof(result_inner_bytes),
            &result_inner_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            2U,
            result_inner_bytes,
            result_inner_length,
            result_bytes,
            sizeof(result_bytes),
            &result_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            2U,
            integer_zero_value,
            sizeof(integer_zero_value),
            result_source_inner_bytes,
            sizeof(result_source_inner_bytes),
            &result_source_inner_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            result_source_inner_bytes,
            result_source_inner_length,
            result_source_choice_bytes,
            sizeof(result_source_choice_bytes),
            &result_source_choice_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            3U,
            result_source_choice_bytes,
            result_source_choice_length,
            result_source_bytes,
            sizeof(result_source_bytes),
            &result_source_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            initiate_response_bytes,
            initiate_response_length,
            initiate_response_wrapper_bytes,
            sizeof(initiate_response_wrapper_bytes),
            &initiate_response_wrapper_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            2U,
            (const uint8_t[]){ 0x03U },
            1U,
            indirect_reference_bytes,
            sizeof(indirect_reference_bytes),
            &indirect_reference_length,
            diagnostic)) {
        return 0;
    }
    if (indirect_reference_length + initiate_response_wrapper_length > sizeof(association_data_content_bytes)) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept ACSE association-data payload is too large.");
        return 0;
    }
    memcpy(association_data_content_bytes, indirect_reference_bytes, indirect_reference_length);
    memcpy(association_data_content_bytes + indirect_reference_length, initiate_response_wrapper_bytes, initiate_response_wrapper_length);
    association_data_content_length = indirect_reference_length + initiate_response_wrapper_length;

    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            8U,
            association_data_content_bytes,
            association_data_content_length,
            association_data_bytes,
            sizeof(association_data_bytes),
            &association_data_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            30U,
            association_data_bytes,
            association_data_length,
            user_information_bytes,
            sizeof(user_information_bytes),
            &user_information_length,
            diagnostic)) {
        return 0;
    }

    sequence_length = 0U;
    if (application_context_name_length > sizeof(sequence_bytes) - sequence_length) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept ACSE sequence payload is too large.");
        return 0;
    }
    memcpy(&sequence_bytes[sequence_length], application_context_name_bytes, application_context_name_length);
    sequence_length += application_context_name_length;
    if (result_length > sizeof(sequence_bytes) - sequence_length) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept ACSE sequence payload is too large.");
        return 0;
    }
    memcpy(&sequence_bytes[sequence_length], result_bytes, result_length);
    sequence_length += result_length;
    if (result_source_length > sizeof(sequence_bytes) - sequence_length) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept ACSE sequence payload is too large.");
        return 0;
    }
    memcpy(&sequence_bytes[sequence_length], result_source_bytes, result_source_length);
    sequence_length += result_source_length;
    if (user_information_length > sizeof(sequence_bytes) - sequence_length) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept ACSE sequence payload is too large.");
        return 0;
    }
    memcpy(&sequence_bytes[sequence_length], user_information_bytes, user_information_length);
    sequence_length += user_information_length;

    if (sequence_length > sizeof(aare_fields_bytes)) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept ACSE AARE payload is too large.");
        return 0;
    }
    memcpy(aare_fields_bytes, sequence_bytes, sequence_length);
    aare_fields_length = sequence_length;

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARE;
    acse_apdu.apdu_bytes = aare_fields_bytes;
    acse_apdu.apdu_length = aare_fields_length;
    return unitlab_mms_acse_encode(&acse_apdu, buffer, buffer_length, encoded_length, diagnostic);
}

static int acse_parse_sequence_or_fields(const uint8_t* buffer, size_t buffer_length, UnitLabMmsAcseApdu* apdu, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement outer_element;
    size_t outer_consumed_length = 0U;

    if (buffer == NULL || apdu == NULL) {
        return 0;
    }

    unitlab_mms_ber_element_init(&outer_element);
    if (!unitlab_mms_ber_read(&outer_element, buffer, buffer_length, &outer_consumed_length, diagnostic)) {
        return 0;
    }
    if (outer_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && outer_element.tag.tag_number == 16U && outer_element.tag.constructed) {
        return acse_parse_raw_fields(outer_element.value_bytes, outer_element.value_length, apdu, diagnostic);
    }
    return acse_parse_raw_fields(buffer, buffer_length, apdu, diagnostic);
}

int unitlab_mms_acse_decode(UnitLabMmsAcseApdu* apdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;
    UnitLabMmsAcseApduKind kind;
    size_t element_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (apdu == NULL || buffer == NULL || consumed_length == NULL) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "ACSE decode requires apdu, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_acse_apdu_init(apdu);
    unitlab_mms_ber_element_init(&element);
    if (!unitlab_mms_ber_read(&element, buffer, buffer_length, &element_consumed_length, diagnostic)) {
        return 0;
    }
    if (!acse_tag_to_kind(&element.tag, &kind)) {
        UnitLabMmsBerElement first_field;
        size_t first_field_consumed_length = 0U;

        unitlab_mms_ber_element_init(&first_field);
        if (!unitlab_mms_ber_read(&first_field, buffer, buffer_length, &first_field_consumed_length, diagnostic)) {
            return 0;
        }
        if (first_field.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC || !first_field.tag.constructed) {
            acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported ACSE APDU tag.");
            return 0;
        }
        kind = UNITLAB_MMS_ACSE_APDU_AARQ;
        apdu->kind = kind;
        apdu->apdu_bytes = buffer;
        apdu->apdu_length = buffer_length;
        apdu->encoded_length = buffer_length;
        if (!acse_parse_raw_fields(buffer, buffer_length, apdu, diagnostic)) {
            unitlab_mms_acse_apdu_init(apdu);
            return 0;
        }
        *consumed_length = buffer_length;
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    apdu->kind = kind;
    apdu->apdu_bytes = element.value_bytes;
    apdu->apdu_length = element.value_length;
    apdu->encoded_length = element.encoded_length;
    if (element.value_length != 0U) {
        if (!acse_parse_sequence_or_fields(element.value_bytes, element.value_length, apdu, diagnostic)) {
            unitlab_mms_acse_apdu_init(apdu);
            return 0;
        }
    }
    *consumed_length = element_consumed_length;
    acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
