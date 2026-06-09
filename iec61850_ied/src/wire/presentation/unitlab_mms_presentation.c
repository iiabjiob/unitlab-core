#include "unitlab_mms_presentation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void presentation_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

static int presentation_encode_fully_encoded_data(
    uint8_t context_identifier,
    const uint8_t* payload_bytes,
    size_t payload_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t* pdv_list_content = NULL;
    uint8_t* pdv_list_bytes = NULL;
    size_t pdv_list_content_capacity = payload_length + 64U;
    size_t pdv_list_capacity = payload_length + 96U;
    uint8_t presentation_context_identifier[] = { 0x00U };
    size_t pdv_list_content_length = 0U;
    size_t context_identifier_length = 0U;
    size_t payload_wrapper_length = 0U;
    size_t pdv_list_length = 0U;
    UnitLabMmsBerElement element;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (context_identifier == 0U) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "presentation context identifier must be non-zero.");
        return 0;
    }
    if (payload_length != 0U && payload_bytes == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "presentation payload bytes are required when payload length is non-zero.");
        return 0;
    }
    if (pdv_list_content_capacity < payload_length || pdv_list_capacity < payload_length) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "presentation fully-encoded-data scratch size overflow.");
        return 0;
    }
    pdv_list_content = (uint8_t*)malloc(pdv_list_content_capacity);
    pdv_list_bytes = (uint8_t*)malloc(pdv_list_capacity);
    if (pdv_list_content == NULL || pdv_list_bytes == NULL) {
        free(pdv_list_content);
        free(pdv_list_bytes);
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "presentation fully-encoded-data scratch allocation failed.");
        return 0;
    }

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    element.tag.constructed = 0;
    element.tag.tag_number = 2U;
    presentation_context_identifier[0] = context_identifier;
    element.value_bytes = presentation_context_identifier;
    element.value_length = sizeof(presentation_context_identifier);
    if (!unitlab_mms_ber_write(&element, pdv_list_content, pdv_list_content_capacity, &context_identifier_length, diagnostic)) {
        free(pdv_list_content);
        free(pdv_list_bytes);
        return 0;
    }

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    element.tag.constructed = 1;
    element.tag.tag_number = 0U;
    element.value_bytes = payload_bytes;
    element.value_length = payload_length;
    if (!unitlab_mms_ber_write(&element, pdv_list_content + context_identifier_length, pdv_list_content_capacity - context_identifier_length, &payload_wrapper_length, diagnostic)) {
        free(pdv_list_content);
        free(pdv_list_bytes);
        return 0;
    }
    pdv_list_content_length = context_identifier_length + payload_wrapper_length;

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    element.tag.constructed = 1;
    element.tag.tag_number = 16U;
    element.value_bytes = pdv_list_content;
    element.value_length = pdv_list_content_length;
    if (!unitlab_mms_ber_write(&element, pdv_list_bytes, pdv_list_capacity, &pdv_list_length, diagnostic)) {
        free(pdv_list_content);
        free(pdv_list_bytes);
        return 0;
    }

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
    element.tag.constructed = 1;
    element.tag.tag_number = 1U;
    element.value_bytes = pdv_list_bytes;
    element.value_length = pdv_list_length;
    if (!unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic)) {
        free(pdv_list_content);
        free(pdv_list_bytes);
        return 0;
    }

    free(pdv_list_content);
    free(pdv_list_bytes);
    presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

static int presentation_decode_fully_encoded_data(
    UnitLabMmsPresentationApdu* apdu,
    const UnitLabMmsBerElement* outer_element,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement pdv_list_element;
    UnitLabMmsBerElement next_element;
    UnitLabMmsBerElement data_element;
    size_t consumed_length = 0U;
    size_t next_consumed_length = 0U;
    size_t data_consumed_length = 0U;
    size_t offset = 0U;

    if (apdu == NULL || outer_element == NULL) {
        return 0;
    }
    if (outer_element->value_length == 0U || outer_element->value_bytes == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation fully-encoded-data is missing PDV-list bytes.");
        return 0;
    }

    unitlab_mms_ber_element_init(&pdv_list_element);
    if (!unitlab_mms_ber_read(&pdv_list_element, outer_element->value_bytes, outer_element->value_length, &consumed_length, diagnostic)) {
        return 0;
    }
    if (consumed_length != outer_element->value_length) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation fully-encoded-data contains trailing bytes.");
        return 0;
    }
    if (pdv_list_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL || pdv_list_element.tag.tag_number != 16U || !pdv_list_element.tag.constructed) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation fully-encoded-data is missing a PDV-list SEQUENCE.");
        return 0;
    }
    if (pdv_list_element.value_length == 0U || pdv_list_element.value_bytes == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation PDV-list is empty.");
        return 0;
    }

    unitlab_mms_ber_element_init(&next_element);
    if (!unitlab_mms_ber_read(&next_element, pdv_list_element.value_bytes, pdv_list_element.value_length, &next_consumed_length, diagnostic)) {
        return 0;
    }

    if (next_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL || next_element.tag.tag_number != 2U || next_element.tag.constructed != 0) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation PDV-list is missing a presentation-context-identifier INTEGER.");
        return 0;
    }
    if (next_element.value_length != 1U || next_element.value_bytes == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation context identifier must fit in one octet.");
        return 0;
    }
    offset += next_consumed_length;
    if (offset >= pdv_list_element.value_length) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation PDV-list is missing presentation-data-values.");
        return 0;
    }

    unitlab_mms_ber_element_init(&data_element);
    if (!unitlab_mms_ber_read(&data_element, pdv_list_element.value_bytes + offset, pdv_list_element.value_length - offset, &data_consumed_length, diagnostic)) {
        return 0;
    }
    if (data_consumed_length != pdv_list_element.value_length - offset) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation PDV-list contains trailing presentation-data-values bytes.");
        return 0;
    }
    if (data_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC || !data_element.tag.constructed || data_element.tag.tag_number > 2U) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation PDV-list contains unsupported presentation-data-values.");
        return 0;
    }

    unitlab_mms_presentation_apdu_init(apdu);
    apdu->kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    apdu->tag = outer_element->tag;
    apdu->context_identifier = next_element.value_bytes[0];
    apdu->payload_bytes = data_element.value_bytes;
    apdu->payload_length = data_element.value_length;
    apdu->encoded_length = outer_element->encoded_length;
    presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

static int presentation_find_wrapped_user_data(
    const UnitLabMmsBerElement* outer_element,
    UnitLabMmsBerElement* user_data_element,
    UnitLabMmsDiagnostic* diagnostic,
    unsigned depth)
{
    size_t offset = 0U;

    if (outer_element == NULL || user_data_element == NULL) {
        return 0;
    }
    if (outer_element->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION
        && (outer_element->tag.tag_number == 0U || outer_element->tag.tag_number == 1U)) {
        *user_data_element = *outer_element;
        return 1;
    }
    if (!outer_element->tag.constructed || outer_element->value_bytes == NULL || outer_element->value_length == 0U) {
        return 0;
    }
    if (depth >= 6U) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "presentation CP-type wrapper nesting is too deep.");
        return 0;
    }

    while (offset < outer_element->value_length) {
        UnitLabMmsBerElement child_element;
        size_t child_consumed_length = 0U;

        unitlab_mms_ber_element_init(&child_element);
        if (!unitlab_mms_ber_read(&child_element, outer_element->value_bytes + offset, outer_element->value_length - offset, &child_consumed_length, diagnostic)) {
            return 0;
        }
        if (child_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION
            && (child_element.tag.tag_number == 0U || child_element.tag.tag_number == 1U)) {
            *user_data_element = child_element;
            return 1;
        }
        if (child_element.tag.constructed && child_element.value_bytes != NULL && child_element.value_length != 0U) {
            if (presentation_find_wrapped_user_data(&child_element, user_data_element, diagnostic, depth + 1U)) {
                return 1;
            }
        }
        offset += child_consumed_length;
    }

    presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "presentation CP-type wrapper does not contain supported user-data.");
    return 0;
}

void unitlab_mms_presentation_apdu_init(UnitLabMmsPresentationApdu* apdu)
{
    if (apdu == NULL) {
        return;
    }
    memset(apdu, 0, sizeof(*apdu));
    unitlab_mms_ber_tag_init(&apdu->tag);
    apdu->context_identifier = 1U;
    apdu->kind = UNITLAB_MMS_PRESENTATION_APDU_NONE;
}

int unitlab_mms_presentation_encode(const UnitLabMmsPresentationApdu* apdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (apdu == NULL || buffer == NULL || encoded_length == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "presentation encode requires apdu, buffer, and encoded_length.");
        return 0;
    }
    if (apdu->kind != UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED && apdu->kind != UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "presentation encode supports simply-encoded-data and fully-encoded-data only.");
        return 0;
    }
    if (apdu->payload_length != 0U && apdu->payload_bytes == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "presentation payload bytes are required when payload length is non-zero.");
        return 0;
    }

    if (apdu->kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED) {
        unitlab_mms_ber_element_init(&element);
        element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
        element.tag.constructed = 0;
        element.tag.tag_number = 0U;
        element.value_bytes = apdu->payload_bytes;
        element.value_length = apdu->payload_length;
        if (!unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic)) {
            return 0;
        }
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }

    if (!presentation_encode_fully_encoded_data(apdu->context_identifier, apdu->payload_bytes, apdu->payload_length, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}

int unitlab_mms_presentation_decode(UnitLabMmsPresentationApdu* apdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;
    size_t element_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (apdu == NULL || buffer == NULL || consumed_length == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "presentation decode requires apdu, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_presentation_apdu_init(apdu);
    unitlab_mms_ber_element_init(&element);
    if (!unitlab_mms_ber_read(&element, buffer, buffer_length, &element_consumed_length, diagnostic)) {
        return 0;
    }
    if (element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION && element.tag.tag_number == 0U && element.tag.constructed == 0) {
        apdu->kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
        apdu->tag = element.tag;
        apdu->context_identifier = 1U;
        apdu->payload_bytes = element.value_bytes;
        apdu->payload_length = element.value_length;
        apdu->encoded_length = element.encoded_length;
        *consumed_length = element_consumed_length;
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION && element.tag.tag_number == 1U && element.tag.constructed == 1) {
        if (!presentation_decode_fully_encoded_data(apdu, &element, diagnostic)) {
            return 0;
        }
        *consumed_length = element_consumed_length;
        return 1;
    }
    if (element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
        && element.tag.constructed
        && (element.tag.tag_number == 16U || element.tag.tag_number == 17U)) {
        UnitLabMmsBerElement user_data_element;

        unitlab_mms_ber_element_init(&user_data_element);
        if (!presentation_find_wrapped_user_data(&element, &user_data_element, diagnostic, 0U)) {
            return 0;
        }
        if (user_data_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION && user_data_element.tag.tag_number == 0U && user_data_element.tag.constructed == 0) {
            apdu->kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
            apdu->tag = user_data_element.tag;
            apdu->context_identifier = 1U;
            apdu->payload_bytes = user_data_element.value_bytes;
            apdu->payload_length = user_data_element.value_length;
            apdu->encoded_length = element_consumed_length;
            *consumed_length = element_consumed_length;
            presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
            return 1;
        }
        if (user_data_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION && user_data_element.tag.tag_number == 1U && user_data_element.tag.constructed == 1) {
            if (!presentation_decode_fully_encoded_data(apdu, &user_data_element, diagnostic)) {
                return 0;
            }
            *consumed_length = element_consumed_length;
            return 1;
        }

        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "presentation CP-type wrapper contains unsupported user-data.");
        return 0;
    }

    if (diagnostic != NULL) {
        snprintf(diagnostic->message, sizeof(diagnostic->message), "presentation user-data tag is unsupported (class=%u constructed=%u tag=%u)", (unsigned)element.tag.tag_class, (unsigned)element.tag.constructed, (unsigned)element.tag.tag_number);
        diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED;
    }
    return 0;
}
