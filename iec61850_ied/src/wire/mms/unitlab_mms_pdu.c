#include "unitlab_mms_pdu.h"

#include <string.h>

static void pdu_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

static int pdu_kind_to_tag(UnitLabMmsPduKind kind, UnitLabMmsBerTag* tag)
{
    if (tag == NULL) {
        return 0;
    }
    unitlab_mms_ber_tag_init(tag);
    tag->tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    switch (kind) {
        case UNITLAB_MMS_PDU_CONFIRMED_REQUEST:
            tag->constructed = 1;
            tag->tag_number = 0U;
            return 1;
        case UNITLAB_MMS_PDU_CONFIRMED_RESPONSE:
            tag->constructed = 1;
            tag->tag_number = 1U;
            return 1;
        case UNITLAB_MMS_PDU_CONFIRMED_ERROR:
            tag->constructed = 1;
            tag->tag_number = 2U;
            return 1;
        case UNITLAB_MMS_PDU_UNCONFIRMED:
            tag->constructed = 1;
            tag->tag_number = 3U;
            return 1;
        case UNITLAB_MMS_PDU_REJECT:
            tag->constructed = 1;
            tag->tag_number = 4U;
            return 1;
        case UNITLAB_MMS_PDU_INITIATE_REQUEST:
            tag->constructed = 1;
            tag->tag_number = 8U;
            return 1;
        case UNITLAB_MMS_PDU_INITIATE_RESPONSE:
            tag->constructed = 1;
            tag->tag_number = 9U;
            return 1;
        case UNITLAB_MMS_PDU_INITIATE_ERROR:
            tag->constructed = 1;
            tag->tag_number = 10U;
            return 1;
        case UNITLAB_MMS_PDU_CONCLUDE_REQUEST:
            tag->constructed = 0;
            tag->tag_number = 11U;
            return 1;
        case UNITLAB_MMS_PDU_CONCLUDE_RESPONSE:
            tag->constructed = 0;
            tag->tag_number = 12U;
            return 1;
        case UNITLAB_MMS_PDU_CONCLUDE_ERROR:
            tag->constructed = 0;
            tag->tag_number = 13U;
            return 1;
        default:
            return 0;
    }
}

static int pdu_tag_to_kind(const UnitLabMmsBerTag* tag, UnitLabMmsPduKind* kind)
{
    if (tag == NULL || kind == NULL) {
        return 0;
    }
    if (tag->tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
        return 0;
    }
    switch (tag->tag_number) {
        case 0U:
            if (tag->constructed) { *kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST; return 1; }
            return 0;
        case 1U:
            if (tag->constructed) { *kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE; return 1; }
            return 0;
        case 2U:
            if (tag->constructed) { *kind = UNITLAB_MMS_PDU_CONFIRMED_ERROR; return 1; }
            return 0;
        case 3U:
            if (tag->constructed) { *kind = UNITLAB_MMS_PDU_UNCONFIRMED; return 1; }
            return 0;
        case 4U:
            if (tag->constructed) { *kind = UNITLAB_MMS_PDU_REJECT; return 1; }
            return 0;
        case 8U:
            if (tag->constructed) { *kind = UNITLAB_MMS_PDU_INITIATE_REQUEST; return 1; }
            return 0;
        case 9U:
            if (tag->constructed) { *kind = UNITLAB_MMS_PDU_INITIATE_RESPONSE; return 1; }
            return 0;
        case 10U:
            if (tag->constructed) { *kind = UNITLAB_MMS_PDU_INITIATE_ERROR; return 1; }
            return 0;
        case 11U:
            if (!tag->constructed) { *kind = UNITLAB_MMS_PDU_CONCLUDE_REQUEST; return 1; }
            return 0;
        case 12U:
            if (!tag->constructed) { *kind = UNITLAB_MMS_PDU_CONCLUDE_RESPONSE; return 1; }
            return 0;
        case 13U:
            if (!tag->constructed) { *kind = UNITLAB_MMS_PDU_CONCLUDE_ERROR; return 1; }
            return 0;
        default:
            return 0;
    }
}

static UnitLabMmsServiceKind pdu_classify_service_kind(UnitLabMmsPduKind kind, const UnitLabMmsBerTag* service_tag)
{
    if (service_tag == NULL) {
        return UNITLAB_MMS_SERVICE_NONE;
    }
    if (service_tag->tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
        return UNITLAB_MMS_SERVICE_RAW;
    }
    if ((kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST || kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE) && service_tag->tag_number == 1U) {
        return UNITLAB_MMS_SERVICE_GET_NAME_LIST;
    }
    if ((kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST || kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE) && service_tag->tag_number == 4U) {
        return UNITLAB_MMS_SERVICE_READ;
    }
    if ((kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST || kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE) && service_tag->tag_number == 5U) {
        return UNITLAB_MMS_SERVICE_WRITE;
    }
    if ((kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST || kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE) && service_tag->tag_number == 6U) {
        return UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES;
    }
    if ((kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST || kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE) && service_tag->tag_number == 12U) {
        return UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES;
    }
    if (kind == UNITLAB_MMS_PDU_UNCONFIRMED && (service_tag->tag_number == 0U || service_tag->tag_number == 3U)) {
        return UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    }
    return UNITLAB_MMS_SERVICE_RAW;
}

static int pdu_decode_invoke_id(const UnitLabMmsBerElement* element, UnitLabMmsBerTagClass expected_tag_class, uint32_t expected_tag_number, uint32_t* invoke_id, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement child;
    size_t consumed_length = 0U;
    uint32_t value = 0U;

    if (invoke_id == NULL || element == NULL) {
        return 0;
    }
    if (element->value_length == 0U || element->value_bytes == NULL) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS confirmed PDU is missing invokeID.");
        return 0;
    }
    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, element->value_bytes, element->value_length, &consumed_length, diagnostic)) {
        return 0;
    }
    if (consumed_length == 0U || consumed_length > element->value_length) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS invokeID element is truncated.");
        return 0;
    }
    if (child.tag.tag_class != expected_tag_class || child.tag.tag_number != expected_tag_number) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS invokeID element has an unexpected tag.");
        return 0;
    }
    if (expected_tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && child.tag.constructed != 0) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS invokeID element is not an INTEGER.");
        return 0;
    }
    if (child.value_length == 0U || child.value_length > 5U) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS invokeID length is invalid.");
        return 0;
    }
    if (child.value_length == 1U) {
        if (child.value_bytes[0] > 0x7FU) {
            pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS invokeID must be encoded as a non-negative integer.");
            return 0;
        }
    } else {
        if (child.value_bytes[0] == 0U) {
            if (child.value_length < 2U || child.value_bytes[1] < 0x80U) {
                pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS invokeID uses a non-minimal leading zero.");
                return 0;
            }
        } else if (child.value_bytes[0] > 0x7FU) {
            pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS invokeID must be encoded as a non-negative integer.");
            return 0;
        }
    }
    if (child.value_length == 5U && child.value_bytes[0] != 0U) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS invokeID overflows Unsigned32.");
        return 0;
    }
    {
        size_t start_index = 0U;
        if (child.value_length == 5U) {
            start_index = 1U;
        }
        for (size_t i = start_index; i < child.value_length; i++) {
            if (value > (UINT32_MAX >> 8U)) {
                pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS invokeID overflows Unsigned32.");
                return 0;
            }
            value = (value << 8U) | (uint32_t)child.value_bytes[i];
        }
    }
    *invoke_id = value;
    return 1;
}

void unitlab_mms_pdu_init(UnitLabMmsPdu* pdu)
{
    if (pdu == NULL) {
        return;
    }
    memset(pdu, 0, sizeof(*pdu));
    pdu->kind = UNITLAB_MMS_PDU_NONE;
    pdu->service_kind = UNITLAB_MMS_SERVICE_NONE;
}

int unitlab_mms_pdu_encode(const UnitLabMmsPdu* pdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;
    UnitLabMmsBerTag tag;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (pdu == NULL || buffer == NULL || encoded_length == NULL) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "MMS PDU encode requires pdu, buffer, and encoded_length.");
        return 0;
    }
    if (pdu->pdu_length != 0U && pdu->pdu_bytes == NULL) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "MMS PDU bytes are required when PDU length is non-zero.");
        return 0;
    }
    if (!pdu_kind_to_tag(pdu->kind, &tag)) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported MMS PDU kind.");
        return 0;
    }
    unitlab_mms_ber_element_init(&element);
    element.tag = tag;
    element.value_bytes = pdu->pdu_bytes;
    element.value_length = pdu->pdu_length;
    if (!unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_pdu_decode(UnitLabMmsPdu* pdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;
    UnitLabMmsPduKind kind;
    size_t element_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (pdu == NULL || buffer == NULL || consumed_length == NULL) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "MMS PDU decode requires pdu, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_pdu_init(pdu);
    unitlab_mms_ber_element_init(&element);
    if (!unitlab_mms_ber_read(&element, buffer, buffer_length, &element_consumed_length, diagnostic)) {
        return 0;
    }
    if (!pdu_tag_to_kind(&element.tag, &kind)) {
        pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported MMS PDU tag.");
        return 0;
    }
    pdu->kind = kind;
    pdu->pdu_bytes = element.value_bytes;
    pdu->pdu_length = element.value_length;
    pdu->encoded_length = element.encoded_length;
    if (kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST || kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE || kind == UNITLAB_MMS_PDU_CONFIRMED_ERROR) {
        UnitLabMmsBerElement service_element;
        size_t service_consumed_length = 0U;
        size_t offset = 0U;

        if (!pdu_decode_invoke_id(&element, kind == UNITLAB_MMS_PDU_CONFIRMED_ERROR ? UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC : UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, kind == UNITLAB_MMS_PDU_CONFIRMED_ERROR ? 0U : 2U, &pdu->invoke_id, diagnostic)) {
            unitlab_mms_pdu_init(pdu);
            return 0;
        }
        pdu->has_invoke_id = 1;
        unitlab_mms_ber_element_init(&service_element);
        if (!unitlab_mms_ber_read(&service_element, element.value_bytes, element.value_length, &offset, diagnostic)) {
            unitlab_mms_pdu_init(pdu);
            return 0;
        }
        service_consumed_length = offset;
        if (service_consumed_length >= element.value_length) {
            pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS confirmed PDU is missing a service choice.");
            unitlab_mms_pdu_init(pdu);
            return 0;
        }
        unitlab_mms_ber_element_init(&service_element);
        if (!unitlab_mms_ber_read(&service_element, &element.value_bytes[service_consumed_length], element.value_length - service_consumed_length, &offset, diagnostic)) {
            unitlab_mms_pdu_init(pdu);
            return 0;
        }
        if (offset > element.value_length - service_consumed_length) {
            pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS confirmed PDU contains trailing service bytes.");
            unitlab_mms_pdu_init(pdu);
            return 0;
        }
        if (service_consumed_length + offset != element.value_length) {
            pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS confirmed PDU contains trailing service bytes.");
            unitlab_mms_pdu_init(pdu);
            return 0;
        }
        pdu->service_tag = service_element.tag;
        pdu->has_service = 1;
        pdu->service_kind = pdu_classify_service_kind(pdu->kind, &service_element.tag);
        pdu->service_bytes = service_element.value_bytes;
        pdu->service_length = service_element.value_length;
    } else if (kind == UNITLAB_MMS_PDU_UNCONFIRMED) {
        UnitLabMmsBerElement service_element;
        size_t offset = 0U;
        unitlab_mms_ber_element_init(&service_element);
        if (!unitlab_mms_ber_read(&service_element, element.value_bytes, element.value_length, &offset, diagnostic)) {
            unitlab_mms_pdu_init(pdu);
            return 0;
        }
        if (offset != element.value_length) {
            pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "MMS unconfirmed PDU contains trailing service bytes.");
            unitlab_mms_pdu_init(pdu);
            return 0;
        }
        pdu->service_tag = service_element.tag;
        pdu->has_service = 1;
        pdu->service_kind = pdu_classify_service_kind(pdu->kind, &service_element.tag);
        pdu->service_bytes = service_element.value_bytes;
        pdu->service_length = service_element.value_length;
    }
    *consumed_length = element_consumed_length;
    pdu_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
