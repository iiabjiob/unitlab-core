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

static int acse_append_bytes(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* offset,
    const uint8_t* bytes,
    size_t bytes_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (buffer == NULL || offset == NULL) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept ACSE requires valid output buffers.");
        return 0;
    }
    if (bytes_length != 0U && bytes == NULL) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association accept ACSE requires value bytes when length is non-zero.");
        return 0;
    }
    if (*offset > buffer_length || bytes_length > buffer_length - *offset) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept ACSE buffer is too small.");
        return 0;
    }
    if (bytes_length != 0U) {
        memcpy(&buffer[*offset], bytes, bytes_length);
    }
    *offset += bytes_length;
    return 1;
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
    uint8_t external_choice_bytes[288U];
    uint8_t external_bytes[320U];
    uint8_t user_data_integer_bytes[16U];
    uint8_t user_information_bytes[352U];
    uint8_t oid_value_bytes[16U];
    uint8_t a1_oid_bytes[32U];
    uint8_t a2_integer_bytes[16U];
    uint8_t a2_bytes[32U];
    uint8_t a3_inner_integer_bytes[16U];
    uint8_t a3_inner_bytes[16U];
    uint8_t a3_bytes[32U];
    uint8_t app_inner_bytes[352U];
    uint8_t app_wrapper_bytes[384U];
    uint8_t a0_wrapper_bytes[416U];
    uint8_t outer_sequence_bytes[448U];
    uint8_t outer_sequence_wrapped[512U];
    uint8_t outer_result_bytes[16U];
    size_t external_choice_length = 0U;
    size_t external_length = 0U;
    size_t user_data_integer_length = 0U;
    size_t user_information_length = 0U;
    size_t oid_value_length = 0U;
    size_t a1_oid_length = 0U;
    size_t a2_integer_length = 0U;
    size_t a2_length = 0U;
    size_t a3_inner_integer_length = 0U;
    size_t a3_inner_length = 0U;
    size_t a3_length = 0U;
    size_t app_inner_length = 0U;
    size_t app_wrapper_length = 0U;
    size_t a0_wrapper_length = 0U;
    size_t outer_sequence_length = 0U;
    size_t outer_sequence_wrapped_length = 0U;
    size_t outer_result_length = 0U;
    UnitLabMmsAcseApdu acse_apdu;
    const uint8_t outer_result_value[] = { 0x01U };
    const uint8_t oid_value[] = { 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U };
    const uint8_t integer_zero_value[] = { 0x00U };
    const uint8_t user_data_integer_value[] = { 0x03U };

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
            2U,
            outer_result_value,
            sizeof(outer_result_value),
            outer_result_bytes,
            sizeof(outer_result_bytes),
            &outer_result_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
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
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            2U,
            user_data_integer_value,
            sizeof(user_data_integer_value),
            user_data_integer_bytes,
            sizeof(user_data_integer_bytes),
            &user_data_integer_length,
            diagnostic)) {
        return 0;
    }
    {
        uint8_t external_value_bytes[320U];
        size_t external_value_length = 0U;
        if (!acse_append_bytes(
                external_value_bytes,
                sizeof(external_value_bytes),
                &external_value_length,
                user_data_integer_bytes,
                user_data_integer_length,
                diagnostic)) {
            return 0;
        }
        if (!acse_append_bytes(
                external_value_bytes,
                sizeof(external_value_bytes),
                &external_value_length,
                external_choice_bytes,
                external_choice_length,
                diagnostic)) {
            return 0;
        }
        if (!acse_encode_nested_element(
                UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
                1,
                8U,
                external_value_bytes,
                external_value_length,
                external_bytes,
                sizeof(external_bytes),
                &external_length,
                diagnostic)) {
            return 0;
        }
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            30U,
            external_bytes,
            external_length,
            user_information_bytes,
            sizeof(user_information_bytes),
            &user_information_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            6U,
            oid_value,
            sizeof(oid_value),
            oid_value_bytes,
            sizeof(oid_value_bytes),
            &oid_value_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            oid_value_bytes,
            oid_value_length,
            a1_oid_bytes,
            sizeof(a1_oid_bytes),
            &a1_oid_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            2U,
            integer_zero_value,
            sizeof(integer_zero_value),
            a2_integer_bytes,
            sizeof(a2_integer_bytes),
            &a2_integer_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            2U,
            a2_integer_bytes,
            a2_integer_length,
            a2_bytes,
            sizeof(a2_bytes),
            &a2_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            2U,
            integer_zero_value,
            sizeof(integer_zero_value),
            a3_inner_integer_bytes,
            sizeof(a3_inner_integer_bytes),
            &a3_inner_integer_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            a3_inner_integer_bytes,
            a3_inner_integer_length,
            a3_inner_bytes,
            sizeof(a3_inner_bytes),
            &a3_inner_length,
            diagnostic)) {
        return 0;
    }
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            3U,
            a3_inner_bytes,
            a3_inner_length,
            a3_bytes,
            sizeof(a3_bytes),
            &a3_length,
            diagnostic)) {
        return 0;
    }
    if (a1_oid_length + a2_length + a3_length + user_information_length > sizeof(app_inner_bytes)) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept ACSE application payload is too large.");
        return 0;
    }
    memcpy(app_inner_bytes, a1_oid_bytes, a1_oid_length);
    memcpy(app_inner_bytes + a1_oid_length, a2_bytes, a2_length);
    memcpy(app_inner_bytes + a1_oid_length + a2_length, a3_bytes, a3_length);
    memcpy(app_inner_bytes + a1_oid_length + a2_length + a3_length, user_information_bytes, user_information_length);
    app_inner_length = a1_oid_length + a2_length + a3_length + user_information_length;
    if (!acse_encode_nested_element(
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
    if (!acse_encode_nested_element(
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
    if (outer_result_length + a0_wrapper_length > sizeof(outer_sequence_bytes)) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association accept ACSE outer sequence is too large.");
        return 0;
    }
    memcpy(outer_sequence_bytes, outer_result_bytes, outer_result_length);
    memcpy(outer_sequence_bytes + outer_result_length, a0_wrapper_bytes, a0_wrapper_length);
    outer_sequence_length = outer_result_length + a0_wrapper_length;
    if (!acse_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            16U,
            outer_sequence_bytes,
            outer_sequence_length,
            outer_sequence_wrapped,
            sizeof(outer_sequence_wrapped),
            &outer_sequence_wrapped_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARE;
    acse_apdu.apdu_bytes = outer_sequence_wrapped;
    acse_apdu.apdu_length = outer_sequence_wrapped_length;
    return unitlab_mms_acse_encode(&acse_apdu, buffer, buffer_length, encoded_length, diagnostic);
}

int unitlab_mms_acse_decode(UnitLabMmsAcseApdu* apdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;
    UnitLabMmsAcseApduKind kind;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (apdu == NULL || buffer == NULL || consumed_length == NULL) {
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "ACSE decode requires apdu, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_ber_element_init(&element);
    if (!unitlab_mms_ber_read(&element, buffer, buffer_length, consumed_length, diagnostic)) {
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
        unitlab_mms_acse_apdu_init(apdu);
        apdu->kind = kind;
        apdu->apdu_bytes = buffer;
        apdu->apdu_length = buffer_length;
        apdu->encoded_length = buffer_length;
        if (!acse_parse_raw_fields(buffer, buffer_length, apdu, diagnostic)) {
            return 0;
        }
        *consumed_length = buffer_length;
        acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    unitlab_mms_acse_apdu_init(apdu);
    apdu->kind = kind;
    apdu->apdu_bytes = element.value_bytes;
    apdu->apdu_length = element.value_length;
    apdu->encoded_length = element.encoded_length;
    if (element.value_length != 0U) {
        if (!acse_parse_raw_fields(element.value_bytes, element.value_length, apdu, diagnostic)) {
            return 0;
        }
    }
    acse_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
