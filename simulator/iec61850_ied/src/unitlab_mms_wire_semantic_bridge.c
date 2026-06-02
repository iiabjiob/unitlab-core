#include "unitlab_mms_wire_semantic_bridge.h"

#include <stdio.h>
#include <string.h>

#include "wire/ber/unitlab_mms_ber.h"

static void bridge_set_diagnostic(UnitLabMmsDecodeDiagnostic* diagnostic, UnitLabMmsDecodeClassification classification, UnitLabMmsDiagnosticCode code, const char* detail, const char* message)
{
    unitlab_mms_decode_diagnostic_set(diagnostic, classification, code, detail, message);
}

static int bridge_extract_last_visible_string(const uint8_t* buffer, size_t buffer_length, char* visible_string, size_t visible_string_size, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsDiagnostic ber_diagnostic;
    UnitLabMmsDiagnostic* ber_diagnostic_ptr = diagnostic != NULL ? &diagnostic->diagnostic : &ber_diagnostic;
    size_t offset = 0U;
    int found = 0;
    char last_visible[256U];

    if (visible_string == NULL || visible_string_size == 0U) {
        return 0;
    }
    visible_string[0] = '\0';
    last_visible[0] = '\0';

    while (offset < buffer_length) {
        UnitLabMmsBerElement element;
        size_t consumed_length = 0U;

        unitlab_mms_ber_element_init(&element);
        if (!unitlab_mms_ber_read(&element, &buffer[offset], buffer_length - offset, &consumed_length, ber_diagnostic_ptr)) {
            return 0;
        }
        if (consumed_length == 0U) {
            break;
        }
        if (element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && element.tag.tag_number == 26U && element.tag.constructed == 0 && element.value_length > 0U) {
            size_t copy_length = element.value_length;
            if (copy_length >= sizeof(last_visible)) {
                copy_length = sizeof(last_visible) - 1U;
            }
            memcpy(last_visible, element.value_bytes, copy_length);
            last_visible[copy_length] = '\0';
            found = 1;
        }
        if (element.tag.constructed && element.value_length > 0U) {
            char nested_visible[256U];
            if (!bridge_extract_last_visible_string(element.value_bytes, element.value_length, nested_visible, sizeof(nested_visible), diagnostic)) {
                return 0;
            }
            if (nested_visible[0] != '\0') {
                snprintf(last_visible, sizeof(last_visible), "%s", nested_visible);
                found = 1;
            }
        }
        offset += consumed_length;
    }

    if (!found) {
        return 1;
    }
    if (strlen(last_visible) >= visible_string_size) {
        return 0;
    }
    snprintf(visible_string, visible_string_size, "%s", last_visible);
    return 1;
}

static int bridge_normalize_read_object_reference(const char* raw, char* object_reference, size_t object_reference_size, char* attribute_reference, size_t attribute_reference_size)
{
    const char* start = raw;
    const char* first = NULL;
    const char* second = NULL;
    char normalized[256U];
    size_t normalized_length = 0U;

    if (object_reference == NULL || object_reference_size == 0U || attribute_reference == NULL || attribute_reference_size == 0U) {
        return 0;
    }
    object_reference[0] = '\0';
    attribute_reference[0] = '\0';
    if (raw == NULL || raw[0] == '\0') {
        return 1;
    }

    first = strchr(start, '$');
    if (first != NULL) {
        second = strchr(first + 1, '$');
        if (second != NULL && *(second + 1) != '\0') {
            start = second + 1;
        }
    }

    for (const char* cursor = start; *cursor != '\0'; cursor++) {
        char character = (*cursor == '$') ? '.' : *cursor;
        if (normalized_length + 1U >= sizeof(normalized)) {
            return 0;
        }
        normalized[normalized_length++] = character;
    }
    normalized[normalized_length] = '\0';

    if (normalized_length == 0U) {
        return 1;
    }
    if (normalized_length >= object_reference_size) {
        return 0;
    }
    snprintf(object_reference, object_reference_size, "%s", normalized);

    const char* attribute = strrchr(object_reference, '.');
    if (attribute != NULL && *(attribute + 1) != '\0') {
        attribute++;
        if (strlen(attribute) >= attribute_reference_size) {
            return 0;
        }
        snprintf(attribute_reference, attribute_reference_size, "%s", attribute);
    }
    return 1;
}

static int bridge_extract_read_request_reference(const uint8_t* service_bytes, size_t service_length, UnitLabMmsDecodedPdu* decoded_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    char raw_reference[256U];

    if (decoded_pdu == NULL) {
        return 0;
    }
    decoded_pdu->object_reference[0] = '\0';
    decoded_pdu->attribute_reference[0] = '\0';
    if (service_bytes == NULL || service_length == 0U) {
        return 1;
    }
    if (!bridge_extract_last_visible_string(service_bytes, service_length, raw_reference, sizeof(raw_reference), diagnostic)) {
        return 0;
    }
    return bridge_normalize_read_object_reference(raw_reference, decoded_pdu->object_reference, sizeof(decoded_pdu->object_reference), decoded_pdu->attribute_reference, sizeof(decoded_pdu->attribute_reference));
}

static int bridge_map_pdu_kind(const UnitLabMmsPdu* wire_pdu, UnitLabMmsDecodedPdu* decoded_pdu, UnitLabMmsServiceOutcome* outcome, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    if (wire_pdu == NULL || decoded_pdu == NULL || outcome == NULL) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_DECODE_FAILURE, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "wire MMS PDU bridge requires wire_pdu, decoded_pdu, and outcome.", "wire MMS PDU bridge requires wire_pdu, decoded_pdu, and outcome.");
        return 0;
    }

    unitlab_mms_decoded_pdu_init(decoded_pdu);
    decoded_pdu->invoke_id = wire_pdu->invoke_id;
    decoded_pdu->value_bytes = wire_pdu->service_bytes;
    decoded_pdu->value_length = wire_pdu->service_length;

    switch (wire_pdu->kind) {
        case UNITLAB_MMS_PDU_INITIATE_REQUEST:
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_ASSOCIATE_REQUEST;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
            return 1;
        case UNITLAB_MMS_PDU_INITIATE_RESPONSE:
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_ASSOCIATE_RESPONSE;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
            return 1;
        case UNITLAB_MMS_PDU_CONFIRMED_REQUEST:
            if (!wire_pdu->has_invoke_id || !wire_pdu->has_service) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "confirmed MMS request is missing invokeID or service choice.", "confirmed MMS request is missing invokeID or service choice.");
                return 0;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_READ) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_READ_REQUEST;
                if (!bridge_extract_read_request_reference(wire_pdu->service_bytes, wire_pdu->service_length, decoded_pdu, diagnostic)) {
                    return 0;
                }
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_WRITE) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_WRITE_REQUEST;
                if (!bridge_extract_read_request_reference(wire_pdu->service_bytes, wire_pdu->service_length, decoded_pdu, diagnostic)) {
                    return 0;
                }
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "confirmed MMS request service is unsupported.", "confirmed MMS request service is unsupported.");
            return 0;
        case UNITLAB_MMS_PDU_CONFIRMED_RESPONSE:
            if (!wire_pdu->has_invoke_id || !wire_pdu->has_service) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "confirmed MMS response is missing invokeID or service choice.", "confirmed MMS response is missing invokeID or service choice.");
                return 0;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_READ) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_READ_RESPONSE;
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_WRITE) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_WRITE_RESPONSE;
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "confirmed MMS response service is unsupported.", "confirmed MMS response service is unsupported.");
            return 0;
        case UNITLAB_MMS_PDU_UNCONFIRMED:
            if (!wire_pdu->has_service || wire_pdu->service_kind != UNITLAB_MMS_SERVICE_INFORMATION_REPORT) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unconfirmed MMS service is unsupported.", "unconfirmed MMS service is unsupported.");
                return 0;
            }
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_INFORMATION_REPORT;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
            return 1;
        case UNITLAB_MMS_PDU_REJECT:
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_REJECT;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_REJECT;
            return 1;
        default:
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported MMS PDU kind for semantic bridge.", "unsupported MMS PDU kind for semantic bridge.");
            return 0;
    }
}

int unitlab_mms_semantic_result_from_wire_pdu(UnitLabMmsSemanticResult* result, const UnitLabMmsPdu* wire_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsDecodedPdu decoded_pdu;
    UnitLabMmsServiceOutcome outcome;
    UnitLabMmsDecodeDiagnostic bridge_diagnostic;

    if (result == NULL) {
        return 0;
    }
    unitlab_mms_semantic_result_init(result);
    unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);
    unitlab_mms_decoded_pdu_init(&decoded_pdu);
    outcome = UNITLAB_MMS_SERVICE_OUTCOME_NONE;

    if (!bridge_map_pdu_kind(wire_pdu, &decoded_pdu, &outcome, &bridge_diagnostic)) {
        if (diagnostic != NULL) {
            *diagnostic = bridge_diagnostic;
        }
        result->diagnostic = bridge_diagnostic;
        return 0;
    }

    unitlab_mms_semantic_result_from_decoded_pdu(result, outcome, &decoded_pdu, &bridge_diagnostic);
    if (diagnostic != NULL) {
        *diagnostic = bridge_diagnostic;
    }
    return result->ok;
}
