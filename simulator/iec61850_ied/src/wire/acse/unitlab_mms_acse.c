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
