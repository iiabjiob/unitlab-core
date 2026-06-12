#include "protocols/mms/unitlab_mms_wire_semantic_bridge.h"

#include <stdio.h>
#include <string.h>

#include "wire/ber/unitlab_mms_ber.h"

static void bridge_set_diagnostic(UnitLabMmsDecodeDiagnostic* diagnostic, UnitLabMmsDecodeClassification classification, UnitLabMmsDiagnosticCode code, const char* detail, const char* message)
{
    unitlab_mms_decode_diagnostic_set(diagnostic, classification, code, detail, message);
}

static int bridge_normalize_item_id(const char* item_id, char* normalized_item, size_t normalized_item_size)
{
    size_t output_length = 0U;

    if (normalized_item == NULL || normalized_item_size == 0U) {
        return 0;
    }
    normalized_item[0] = '\0';
    if (item_id == NULL || item_id[0] == '\0') {
        return 1;
    }
    for (const char* cursor = item_id; *cursor != '\0'; cursor++) {
        char character = (*cursor == '$') ? '.' : *cursor;
        if (output_length + 1U >= normalized_item_size) {
            return 0;
        }
        normalized_item[output_length++] = character;
    }
    normalized_item[output_length] = '\0';
    return 1;
}

static int bridge_copy_bytes_as_string(const uint8_t* bytes, size_t length, char* output, size_t output_size);

static int bridge_build_canonical_object_reference(const char* domain_id, const char* item_id, char* object_reference, size_t object_reference_size, char* attribute_reference, size_t attribute_reference_size)
{
    char normalized_item[128U];
    size_t object_reference_length = 0U;
    const char* attribute = NULL;

    if (object_reference == NULL || object_reference_size == 0U || attribute_reference == NULL || attribute_reference_size == 0U) {
        return 0;
    }
    object_reference[0] = '\0';
    attribute_reference[0] = '\0';
    if (!bridge_normalize_item_id(item_id, normalized_item, sizeof(normalized_item))) {
        return 0;
    }
    if (domain_id != NULL && domain_id[0] != '\0') {
        size_t domain_length = strlen(domain_id);
        if (domain_length + 1U >= object_reference_size) {
            return 0;
        }
        memcpy(object_reference, domain_id, domain_length);
        object_reference_length = domain_length;
        object_reference[object_reference_length++] = '.';
    }
    if (object_reference_length + strlen(normalized_item) >= object_reference_size) {
        return 0;
    }
    memcpy(object_reference + object_reference_length, normalized_item, strlen(normalized_item));
    object_reference_length += strlen(normalized_item);
    object_reference[object_reference_length] = '\0';

    attribute = strrchr(object_reference, '.');
    if (attribute != NULL && *(attribute + 1) != '\0') {
        attribute++;
    } else {
        attribute = object_reference;
    }
    if (strlen(attribute) >= attribute_reference_size) {
        return 0;
    }
    snprintf(attribute_reference, attribute_reference_size, "%s", attribute);
    return 1;
}

static int bridge_parse_object_name_wrapper(const UnitLabMmsBerElement* wrapper, UnitLabMmsDecodedPdu* decoded_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsBerElement object_name_element;
    UnitLabMmsBerElement child;
    size_t object_name_consumed_length = 0U;
    size_t child_consumed_length = 0U;
    size_t offset = 0U;

    if (wrapper == NULL || decoded_pdu == NULL) {
        return 0;
    }
    decoded_pdu->domain_id[0] = '\0';
    decoded_pdu->item_id[0] = '\0';
    decoded_pdu->object_reference[0] = '\0';
    decoded_pdu->attribute_reference[0] = '\0';
    unitlab_mms_ber_element_init(&object_name_element);
    if (!unitlab_mms_ber_read(&object_name_element, wrapper->value_bytes, wrapper->value_length, &object_name_consumed_length, &diagnostic->diagnostic)) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "variableSpecification objectName wrapper is malformed.", "variableSpecification objectName wrapper is malformed.");
        return 0;
    }
    if (object_name_consumed_length != wrapper->value_length || object_name_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "ObjectName wrapper is malformed.", "ObjectName wrapper is malformed.");
        return 0;
    }
    if (object_name_element.tag.tag_number == 0U && !object_name_element.tag.constructed) {
        if (!bridge_copy_bytes_as_string(object_name_element.value_bytes, object_name_element.value_length, decoded_pdu->item_id, sizeof(decoded_pdu->item_id))) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "vmdspecific ObjectName is too long.", "vmdspecific ObjectName is too long.");
            return 0;
        }
        if (!bridge_build_canonical_object_reference(NULL, decoded_pdu->item_id, decoded_pdu->object_reference, sizeof(decoded_pdu->object_reference), decoded_pdu->attribute_reference, sizeof(decoded_pdu->attribute_reference))) {
            return 0;
        }
        return 1;
    }
    if (object_name_element.tag.tag_number != 1U || !object_name_element.tag.constructed) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "ObjectName choice is unsupported.", "ObjectName choice is unsupported.");
        return 0;
    }

    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, object_name_element.value_bytes, object_name_element.value_length, &child_consumed_length, &diagnostic->diagnostic)) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "domainspecific ObjectName domainId BER decode failed.", "domainspecific ObjectName domainId BER decode failed.");
        return 0;
    }
    if (child_consumed_length == 0U || child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL || child.tag.tag_number != 26U || child.tag.constructed) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "domainspecific ObjectName domainId is malformed.", "domainspecific ObjectName domainId is malformed.");
        return 0;
    }
    if (!bridge_copy_bytes_as_string(child.value_bytes, child.value_length, decoded_pdu->domain_id, sizeof(decoded_pdu->domain_id))) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "domainspecific ObjectName domainId is too long.", "domainspecific ObjectName domainId is too long.");
        return 0;
    }
    offset += child_consumed_length;

    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, &object_name_element.value_bytes[offset], object_name_element.value_length - offset, &child_consumed_length, &diagnostic->diagnostic)) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "domainspecific ObjectName itemId BER decode failed.", "domainspecific ObjectName itemId BER decode failed.");
        return 0;
    }
    if (child_consumed_length == 0U || child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL || child.tag.tag_number != 26U || child.tag.constructed) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "domainspecific ObjectName itemId is malformed.", "domainspecific ObjectName itemId is malformed.");
        return 0;
    }
    if (!bridge_copy_bytes_as_string(child.value_bytes, child.value_length, decoded_pdu->item_id, sizeof(decoded_pdu->item_id))) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "domainspecific ObjectName itemId is too long.", "domainspecific ObjectName itemId is too long.");
        return 0;
    }
    offset += child_consumed_length;
    if (offset != object_name_element.value_length) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "domainspecific ObjectName contains trailing bytes.", "domainspecific ObjectName contains trailing bytes.");
        return 0;
    }
    if (!bridge_build_canonical_object_reference(decoded_pdu->domain_id, decoded_pdu->item_id, decoded_pdu->object_reference, sizeof(decoded_pdu->object_reference), decoded_pdu->attribute_reference, sizeof(decoded_pdu->attribute_reference))) {
        return 0;
    }
    return 1;
}

static int bridge_store_read_target(UnitLabMmsDecodedPdu* decoded_pdu, const UnitLabMmsDecodedPdu* target_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    size_t index = 0U;

    if (decoded_pdu == NULL || target_pdu == NULL) {
        return 0;
    }
    if (decoded_pdu->read_object_reference_count >= UNITLAB_MMS_MAX_READ_VARIABLES) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "read request contains too many variableSpecifications.", "read request contains too many variableSpecifications.");
        return 0;
    }
    index = decoded_pdu->read_object_reference_count;
    if (index == 0U) {
        snprintf(decoded_pdu->domain_id, sizeof(decoded_pdu->domain_id), "%s", target_pdu->domain_id);
        snprintf(decoded_pdu->item_id, sizeof(decoded_pdu->item_id), "%s", target_pdu->item_id);
        snprintf(decoded_pdu->object_reference, sizeof(decoded_pdu->object_reference), "%s", target_pdu->object_reference);
        snprintf(decoded_pdu->attribute_reference, sizeof(decoded_pdu->attribute_reference), "%s", target_pdu->attribute_reference);
    }
    snprintf(decoded_pdu->read_object_references[index], sizeof(decoded_pdu->read_object_references[index]), "%s", target_pdu->object_reference);
    snprintf(decoded_pdu->read_attribute_references[index], sizeof(decoded_pdu->read_attribute_references[index]), "%s", target_pdu->attribute_reference);
    decoded_pdu->read_object_reference_count++;
    return 1;
}

static int bridge_store_write_target(UnitLabMmsDecodedPdu* decoded_pdu, const UnitLabMmsDecodedPdu* target_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    size_t index = 0U;

    if (decoded_pdu == NULL || target_pdu == NULL) {
        return 0;
    }
    if (decoded_pdu->write_object_reference_count >= UNITLAB_MMS_MAX_READ_VARIABLES) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "write request contains too many variableSpecifications.", "write request contains too many variableSpecifications.");
        return 0;
    }
    index = decoded_pdu->write_object_reference_count;
    if (index == 0U) {
        snprintf(decoded_pdu->domain_id, sizeof(decoded_pdu->domain_id), "%s", target_pdu->domain_id);
        snprintf(decoded_pdu->item_id, sizeof(decoded_pdu->item_id), "%s", target_pdu->item_id);
        snprintf(decoded_pdu->object_reference, sizeof(decoded_pdu->object_reference), "%s", target_pdu->object_reference);
        snprintf(decoded_pdu->attribute_reference, sizeof(decoded_pdu->attribute_reference), "%s", target_pdu->attribute_reference);
    }
    snprintf(decoded_pdu->write_object_references[index], sizeof(decoded_pdu->write_object_references[index]), "%s", target_pdu->object_reference);
    snprintf(decoded_pdu->write_attribute_references[index], sizeof(decoded_pdu->write_attribute_references[index]), "%s", target_pdu->attribute_reference);
    decoded_pdu->write_object_reference_count++;
    return 1;
}

static int bridge_store_write_value(UnitLabMmsDecodedPdu* decoded_pdu, const UnitLabMmsBerElement* data_element, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    size_t index = 0U;

    if (decoded_pdu == NULL || data_element == NULL) {
        return 0;
    }
    index = decoded_pdu->value_length;
    if (index >= UNITLAB_MMS_MAX_READ_VARIABLES) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "write request contains too many data values.", "write request contains too many data values.");
        return 0;
    }
    if (data_element->value_length > sizeof(decoded_pdu->write_values[index])) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "write request data value is too large.", "write request data value is too large.");
        return 0;
    }
    memcpy(decoded_pdu->write_values[index], data_element->value_bytes, data_element->value_length);
    decoded_pdu->write_value_lengths[index] = data_element->value_length;
    if (index == 0U) {
        decoded_pdu->value_bytes = decoded_pdu->write_values[index];
        decoded_pdu->value_length = data_element->value_length;
    } else {
        decoded_pdu->value_length = index + 1U;
    }
    return 1;
}

static int bridge_parse_read_request(const uint8_t* service_bytes, size_t service_length, UnitLabMmsDecodedPdu* decoded_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsBerElement read_request_element;
    UnitLabMmsBerElement child;
    UnitLabMmsBerElement variable_access_element;
    UnitLabMmsDecodedPdu target_pdu;
    const uint8_t* fields_bytes = service_bytes;
    size_t fields_length = service_length;
    size_t read_request_consumed_length = 0U;
    size_t child_consumed_length = 0U;
    size_t variable_access_consumed_length = 0U;
    size_t offset = 0U;
    int found_variable_access = 0;

    if (decoded_pdu == NULL) {
        return 0;
    }
    decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_NONE;
    decoded_pdu->correlation_id = 0U;
    decoded_pdu->timestamp_ms = 0U;
    decoded_pdu->deadline_ms = 0U;
    decoded_pdu->object_reference[0] = '\0';
    decoded_pdu->attribute_reference[0] = '\0';
    decoded_pdu->read_object_reference_count = 0U;
    decoded_pdu->report_control_reference[0] = '\0';
    decoded_pdu->data_set_reference[0] = '\0';
    decoded_pdu->object_class = 0U;
    decoded_pdu->object_scope = 0U;
    decoded_pdu->domain_id[0] = '\0';
    decoded_pdu->node_id[0] = '\0';
    decoded_pdu->continue_after[0] = '\0';
    decoded_pdu->item_id[0] = '\0';
    decoded_pdu->value_bytes = NULL;
    decoded_pdu->value_length = 0U;
    decoded_pdu->buffered = 0;
    unitlab_mms_decoded_pdu_init(&target_pdu);
    unitlab_mms_ber_element_init(&read_request_element);
    if (!unitlab_mms_ber_read(&read_request_element, service_bytes, service_length, &read_request_consumed_length, &diagnostic->diagnostic)) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request BER decode failed.", "read request BER decode failed.");
        return 0;
    }
    if (read_request_consumed_length == service_length
        && read_request_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
        && read_request_element.tag.tag_number == 16U
        && read_request_element.tag.constructed) {
        fields_bytes = read_request_element.value_bytes;
        fields_length = read_request_element.value_length;
    }
    while (offset < fields_length) {
        unitlab_mms_ber_element_init(&child);
        if (!unitlab_mms_ber_read(&child, &fields_bytes[offset], fields_length - offset, &child_consumed_length, &diagnostic->diagnostic)) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request top-level field BER decode failed.", "read request top-level field BER decode failed.");
            return 0;
        }
        if (child_consumed_length == 0U) {
            break;
        }
        if (child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request contains a non-context-specific field.", "read request contains a non-context-specific field.");
            return 0;
        }
        if (child.tag.tag_number == 1U) {
            size_t top_level_field_length = child_consumed_length;
            size_t member_offset = 0U;

            unitlab_mms_ber_element_init(&variable_access_element);
            if (!unitlab_mms_ber_read(&variable_access_element, child.value_bytes, child.value_length, &variable_access_consumed_length, &diagnostic->diagnostic)) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request variableAccessSpecification BER decode failed.", "read request variableAccessSpecification BER decode failed.");
                return 0;
            }
            if (variable_access_consumed_length != child.value_length || variable_access_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC || variable_access_element.tag.tag_number != 0U || !variable_access_element.tag.constructed) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request variableAccessSpecification is malformed.", "read request variableAccessSpecification is malformed.");
                return 0;
            }
            while (member_offset < variable_access_element.value_length) {
                UnitLabMmsBerElement member_sequence;
                UnitLabMmsBerElement member_wrapper;
                size_t member_sequence_consumed_length = 0U;
                size_t member_wrapper_consumed_length = 0U;

                unitlab_mms_ber_element_init(&member_sequence);
                if (!unitlab_mms_ber_read(&member_sequence, &variable_access_element.value_bytes[member_offset], variable_access_element.value_length - member_offset, &member_sequence_consumed_length, &diagnostic->diagnostic)) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request variableSpecification BER decode failed.", "read request variableSpecification BER decode failed.");
                    return 0;
                }
                if (member_sequence_consumed_length == 0U) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request variableSpecification is malformed.", "read request variableSpecification is malformed.");
                    return 0;
                }
                if (member_sequence.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && (member_sequence.tag.tag_number == 0U || member_sequence.tag.tag_number == 1U) && member_sequence.tag.constructed) {
                    member_wrapper = member_sequence;
                } else if (member_sequence.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && member_sequence.tag.tag_number == 16U && member_sequence.tag.constructed) {
                    unitlab_mms_ber_element_init(&member_wrapper);
                    if (!unitlab_mms_ber_read(&member_wrapper, member_sequence.value_bytes, member_sequence.value_length, &member_wrapper_consumed_length, &diagnostic->diagnostic)) {
                        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request variableSpecification wrapper BER decode failed.", "read request variableSpecification wrapper BER decode failed.");
                        return 0;
                    }
                    if (member_wrapper_consumed_length != member_sequence.value_length || member_wrapper.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC || (member_wrapper.tag.tag_number != 0U && member_wrapper.tag.tag_number != 1U) || !member_wrapper.tag.constructed) {
                        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request variableSpecification is malformed.", "read request variableSpecification is malformed.");
                        return 0;
                    }
                } else {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request variableSpecification is malformed.", "read request variableSpecification is malformed.");
                    return 0;
                }
                unitlab_mms_decoded_pdu_init(&target_pdu);
                if (!bridge_parse_object_name_wrapper(&member_wrapper, &target_pdu, diagnostic)) {
                    return 0;
                }
                if (!bridge_store_read_target(decoded_pdu, &target_pdu, diagnostic)) {
                    return 0;
                }
                member_offset += member_sequence_consumed_length;
            }
            found_variable_access = 1;
            offset += top_level_field_length;
            continue;
        } else if (child.tag.tag_number != 0U) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "read request field is unsupported.", "read request field is unsupported.");
            return 0;
        }
        offset += child_consumed_length;
        continue;
    }
    if (!found_variable_access || decoded_pdu->read_object_reference_count == 0U) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "read request must include variableAccessSpecification.", "read request must include variableAccessSpecification.");
        return 0;
    }
    decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_READ_REQUEST;
    return 1;
}

static int bridge_parse_write_request(const uint8_t* service_bytes, size_t service_length, UnitLabMmsDecodedPdu* decoded_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsBerElement write_request_element;
    UnitLabMmsBerElement child;
    UnitLabMmsBerElement data_element;
    size_t write_request_consumed_length = 0U;
    size_t child_consumed_length = 0U;
    size_t data_consumed_length = 0U;
    size_t list_consumed_length = 0U;
    size_t offset = 0U;
    const uint8_t* fields_bytes = NULL;
    size_t fields_length = 0U;
    UnitLabMmsBerElement list_element;
    int found_variable_access = 0;
    int found_data = 0;

    if (decoded_pdu == NULL) {
        return 0;
    }
    decoded_pdu->domain_id[0] = '\0';
    decoded_pdu->item_id[0] = '\0';
    decoded_pdu->object_reference[0] = '\0';
    decoded_pdu->attribute_reference[0] = '\0';
    decoded_pdu->value_bytes = NULL;
    decoded_pdu->value_length = 0U;
    decoded_pdu->write_object_reference_count = 0U;
    unitlab_mms_ber_element_init(&write_request_element);
    if (!unitlab_mms_ber_read(&write_request_element, service_bytes, service_length, &write_request_consumed_length, &diagnostic->diagnostic)) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request BER decode failed.", "write request BER decode failed.");
        return 0;
    }
    if (write_request_consumed_length == service_length && write_request_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && write_request_element.tag.tag_number == 16U && write_request_element.tag.constructed) {
        fields_bytes = write_request_element.value_bytes;
        fields_length = write_request_element.value_length;
    }
    else {
        fields_bytes = service_bytes;
        fields_length = service_length;
    }
    while (offset < fields_length) {
        unitlab_mms_ber_element_init(&child);
        if (!unitlab_mms_ber_read(&child, &fields_bytes[offset], fields_length - offset, &child_consumed_length, &diagnostic->diagnostic)) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request top-level field BER decode failed.", "write request top-level field BER decode failed.");
            return 0;
        }
        if (child_consumed_length == 0U) {
            break;
        }
        if (child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request contains a non-context-specific field.", "write request contains a non-context-specific field.");
            return 0;
        }
        if (child.tag.tag_number == 0U && !found_variable_access) {
            size_t top_level_field_length = child_consumed_length;
            size_t member_offset = 0U;

            unitlab_mms_ber_element_init(&list_element);
            if (!unitlab_mms_ber_read(&list_element, child.value_bytes, child.value_length, &list_consumed_length, &diagnostic->diagnostic)) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request listOfVariable BER decode failed.", "write request listOfVariable BER decode failed.");
                return 0;
            }
            if (list_consumed_length == 0U || list_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL || list_element.tag.tag_number != 16U || !list_element.tag.constructed) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request variableAccessSpecification list is malformed.", "write request variableAccessSpecification list is malformed.");
                return 0;
            }
            while (member_offset < child.value_length) {
                UnitLabMmsBerElement member_sequence;
                UnitLabMmsBerElement member_wrapper;
                UnitLabMmsDecodedPdu target_pdu;
                size_t member_sequence_consumed_length = 0U;
                size_t member_wrapper_consumed_length = 0U;

                unitlab_mms_ber_element_init(&member_sequence);
                if (!unitlab_mms_ber_read(&member_sequence, &child.value_bytes[member_offset], child.value_length - member_offset, &member_sequence_consumed_length, &diagnostic->diagnostic)) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request variableSpecification BER decode failed.", "write request variableSpecification BER decode failed.");
                    return 0;
                }
                if (member_sequence_consumed_length == 0U || member_sequence.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL || member_sequence.tag.tag_number != 16U || !member_sequence.tag.constructed) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request variableSpecification is malformed.", "write request variableSpecification is malformed.");
                    return 0;
                }
                unitlab_mms_ber_element_init(&member_wrapper);
                if (!unitlab_mms_ber_read(&member_wrapper, member_sequence.value_bytes, member_sequence.value_length, &member_wrapper_consumed_length, &diagnostic->diagnostic)) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request variableSpecification wrapper BER decode failed.", "write request variableSpecification wrapper BER decode failed.");
                    return 0;
                }
                if (member_wrapper_consumed_length != member_sequence.value_length || member_wrapper.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC || member_wrapper.tag.tag_number != 0U || !member_wrapper.tag.constructed) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request variableSpecification is malformed.", "write request variableSpecification is malformed.");
                    return 0;
                }
                unitlab_mms_decoded_pdu_init(&target_pdu);
                if (!bridge_parse_object_name_wrapper(&member_wrapper, &target_pdu, diagnostic)) {
                    return 0;
                }
                if (!bridge_store_write_target(decoded_pdu, &target_pdu, diagnostic)) {
                    return 0;
                }
                member_offset += member_sequence_consumed_length;
            }
            found_variable_access = 1;
            offset += top_level_field_length;
            continue;
        }
        if (child.tag.tag_number == 0U && found_variable_access && !found_data) {
            size_t top_level_field_length = child_consumed_length;
            size_t data_offset = 0U;
            size_t data_count = 0U;

            while (data_offset < child.value_length) {
                unitlab_mms_ber_element_init(&data_element);
                if (!unitlab_mms_ber_read(&data_element, &child.value_bytes[data_offset], child.value_length - data_offset, &data_consumed_length, &diagnostic->diagnostic)) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request Data element BER decode failed.", "write request Data element BER decode failed.");
                    return 0;
                }
                if (data_consumed_length == 0U) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request Data element is malformed.", "write request Data element is malformed.");
                    return 0;
                }
                decoded_pdu->value_length = data_count;
                if (!bridge_store_write_value(decoded_pdu, &data_element, diagnostic)) {
                    return 0;
                }
                data_count++;
                data_offset += data_consumed_length;
            }
            if (data_count == 1U) {
                decoded_pdu->value_bytes = decoded_pdu->write_values[0];
                decoded_pdu->value_length = decoded_pdu->write_value_lengths[0];
            }
            found_data = 1;
            offset += top_level_field_length;
            continue;
        }
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "write request field is unsupported.", "write request field is unsupported.");
        return 0;
    }
    if (!found_variable_access || !found_data || decoded_pdu->write_object_reference_count == 0U) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request must include variableAccessSpecification and listOfData.", "write request must include variableAccessSpecification and listOfData.");
        return 0;
    }
    if (decoded_pdu->write_object_reference_count != 1U && decoded_pdu->value_length != decoded_pdu->write_object_reference_count) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "write request variable/data counts differ.", "write request variable/data counts differ.");
        return 0;
    }
    decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_WRITE_REQUEST;
    return 1;
}

static int bridge_parse_get_variable_access_attributes_request(const uint8_t* service_bytes, size_t service_length, UnitLabMmsDecodedPdu* decoded_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsBerElement name_element;
    size_t name_consumed_length = 0U;

    if (decoded_pdu == NULL) {
        return 0;
    }
    decoded_pdu->domain_id[0] = '\0';
    decoded_pdu->item_id[0] = '\0';
    decoded_pdu->object_reference[0] = '\0';
    decoded_pdu->attribute_reference[0] = '\0';
    unitlab_mms_ber_element_init(&name_element);
    if (!unitlab_mms_ber_read(&name_element, service_bytes, service_length, &name_consumed_length, &diagnostic->diagnostic)) {
        return 0;
    }
    if (name_consumed_length != service_length || name_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC || name_element.tag.tag_number != 0U || !name_element.tag.constructed) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getVariableAccessAttributes request must use a name wrapper.", "getVariableAccessAttributes request must use a name wrapper.");
        return 0;
    }
    if (!bridge_parse_object_name_wrapper(&name_element, decoded_pdu, diagnostic)) {
        return 0;
    }
    decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_REQUEST;
    return 1;
}

static int bridge_parse_get_named_variable_list_attributes_request(const uint8_t* service_bytes, size_t service_length, UnitLabMmsDecodedPdu* decoded_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsBerElement object_name_element;
    UnitLabMmsBerElement child;
    size_t object_name_consumed_length = 0U;
    size_t child_consumed_length = 0U;
    size_t offset = 0U;

    if (decoded_pdu == NULL) {
        return 0;
    }
    decoded_pdu->domain_id[0] = '\0';
    decoded_pdu->item_id[0] = '\0';
    decoded_pdu->object_reference[0] = '\0';
    decoded_pdu->attribute_reference[0] = '\0';
    unitlab_mms_ber_element_init(&object_name_element);
    if (!unitlab_mms_ber_read(&object_name_element, service_bytes, service_length, &object_name_consumed_length, &diagnostic->diagnostic)) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes request BER decode failed.", "getNamedVariableListAttributes request BER decode failed.");
        return 0;
    }
    if (object_name_consumed_length != service_length || object_name_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes request name is malformed.", "getNamedVariableListAttributes request name is malformed.");
        return 0;
    }
    if (object_name_element.tag.tag_number == 0U && !object_name_element.tag.constructed) {
        if (!bridge_copy_bytes_as_string(object_name_element.value_bytes, object_name_element.value_length, decoded_pdu->item_id, sizeof(decoded_pdu->item_id))) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes list name is too long.", "getNamedVariableListAttributes list name is too long.");
            return 0;
        }
        if (!bridge_copy_bytes_as_string(object_name_element.value_bytes, object_name_element.value_length, decoded_pdu->object_reference, sizeof(decoded_pdu->object_reference))) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes list reference is too long.", "getNamedVariableListAttributes list reference is too long.");
            return 0;
        }
        if (!bridge_copy_bytes_as_string(object_name_element.value_bytes, object_name_element.value_length, decoded_pdu->attribute_reference, sizeof(decoded_pdu->attribute_reference))) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes attribute reference is too long.", "getNamedVariableListAttributes attribute reference is too long.");
            return 0;
        }
        decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_REQUEST;
        return 1;
    }
    if (object_name_element.tag.tag_number != 1U || !object_name_element.tag.constructed) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "getNamedVariableListAttributes request name choice is unsupported.", "getNamedVariableListAttributes request name choice is unsupported.");
        return 0;
    }

    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, object_name_element.value_bytes, object_name_element.value_length, &child_consumed_length, &diagnostic->diagnostic)) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes domainId BER decode failed.", "getNamedVariableListAttributes domainId BER decode failed.");
        return 0;
    }
    if (child_consumed_length == 0U || child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL || child.tag.tag_number != 26U || child.tag.constructed) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes domainId is malformed.", "getNamedVariableListAttributes domainId is malformed.");
        return 0;
    }
    if (!bridge_copy_bytes_as_string(child.value_bytes, child.value_length, decoded_pdu->domain_id, sizeof(decoded_pdu->domain_id))) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes domainId is too long.", "getNamedVariableListAttributes domainId is too long.");
        return 0;
    }
    offset += child_consumed_length;

    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, &object_name_element.value_bytes[offset], object_name_element.value_length - offset, &child_consumed_length, &diagnostic->diagnostic)) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes itemId BER decode failed.", "getNamedVariableListAttributes itemId BER decode failed.");
        return 0;
    }
    if (child_consumed_length == 0U || child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL || child.tag.tag_number != 26U || child.tag.constructed) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes itemId is malformed.", "getNamedVariableListAttributes itemId is malformed.");
        return 0;
    }
    if (!bridge_copy_bytes_as_string(child.value_bytes, child.value_length, decoded_pdu->item_id, sizeof(decoded_pdu->item_id))) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes itemId is too long.", "getNamedVariableListAttributes itemId is too long.");
        return 0;
    }
    offset += child_consumed_length;
    if (offset != object_name_element.value_length) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes request contains trailing bytes.", "getNamedVariableListAttributes request contains trailing bytes.");
        return 0;
    }
    if (snprintf(decoded_pdu->object_reference, sizeof(decoded_pdu->object_reference), "%s/%s", decoded_pdu->domain_id, decoded_pdu->item_id) >= (int)sizeof(decoded_pdu->object_reference)) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes request reference is too long.", "getNamedVariableListAttributes request reference is too long.");
        return 0;
    }
    if (!bridge_copy_bytes_as_string((const uint8_t*)decoded_pdu->item_id, strlen(decoded_pdu->item_id), decoded_pdu->attribute_reference, sizeof(decoded_pdu->attribute_reference))) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNamedVariableListAttributes attribute reference is too long.", "getNamedVariableListAttributes attribute reference is too long.");
        return 0;
    }
    decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_REQUEST;
    return 1;
}


static int bridge_copy_bytes_as_string(const uint8_t* bytes, size_t length, char* output, size_t output_size)
{
    if (output == NULL || output_size == 0U) {
        return 0;
    }
    output[0] = '\0';
    if (length == 0U) {
        return 1;
    }
    if (bytes == NULL || length >= output_size) {
        return 0;
    }
    memcpy(output, bytes, length);
    output[length] = '\0';
    return 1;
}

static int bridge_decode_uint32_bytes(const uint8_t* bytes, size_t length, uint32_t* value)
{
    uint32_t decoded_value = 0U;

    if (bytes == NULL || value == NULL || length == 0U || length > sizeof(uint32_t)) {
        return 0;
    }
    for (size_t index = 0U; index < length; index++) {
        if (decoded_value > (UINT32_MAX >> 8U)) {
            return 0;
        }
        decoded_value = (decoded_value << 8U) | (uint32_t)bytes[index];
    }
    *value = decoded_value;
    return 1;
}

static int bridge_collect_get_name_list_object_class(const UnitLabMmsBerElement* element, uint32_t* object_class, int* has_object_class, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsBerElement child;
    size_t offset = 0U;

    if (element == NULL || object_class == NULL || has_object_class == NULL) {
        return 0;
    }

    if (!element->tag.constructed) {
        uint32_t candidate_value = 0U;

        if (element->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && element->tag.tag_number == 2U) {
            if (!bridge_decode_uint32_bytes(element->value_bytes, element->value_length, &candidate_value)) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList objectClass is malformed.", "getNameList objectClass is malformed.");
                return 0;
            }
            *object_class = candidate_value;
            *has_object_class = 1;
        } else if (element->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && element->tag.tag_number == 0U && element->value_length > 0U) {
            if (!bridge_decode_uint32_bytes(element->value_bytes, element->value_length, &candidate_value)) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList objectClass is malformed.", "getNameList objectClass is malformed.");
                return 0;
            }
            *object_class = candidate_value;
            *has_object_class = 1;
        }
        return 1;
    }

    while (offset < element->value_length) {
        size_t child_consumed_length = 0U;

        unitlab_mms_ber_element_init(&child);
        if (!unitlab_mms_ber_read(&child, &element->value_bytes[offset], element->value_length - offset, &child_consumed_length, &diagnostic->diagnostic)) {
            return 0;
        }
        if (child_consumed_length == 0U) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList request contains a truncated field.", "getNameList request contains a truncated field.");
            return 0;
        }
        if (!bridge_collect_get_name_list_object_class(&child, object_class, has_object_class, diagnostic)) {
            return 0;
        }
        offset += child_consumed_length;
    }
    return 1;
}

static int bridge_collect_get_name_list_object_scope(const UnitLabMmsBerElement* element, uint32_t* object_scope, int* has_object_scope, char* domain_id, size_t domain_id_size, char* node_id, size_t node_id_size, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsBerElement child;
    size_t offset = 0U;

    if (element == NULL || object_scope == NULL || has_object_scope == NULL) {
        return 0;
    }

    if (!element->tag.constructed) {
        if (element->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
            if (element->tag.tag_number == 0U && element->value_length == 0U) {
                *object_scope = 0U;
                *has_object_scope = 1;
                if (domain_id != NULL && domain_id_size > 0U) {
                    domain_id[0] = '\0';
                }
            } else if (element->tag.tag_number == 1U) {
                if (!bridge_copy_bytes_as_string(element->value_bytes, element->value_length, domain_id, domain_id_size)) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList domain identifier is too long.", "getNameList domain identifier is too long.");
                    return 0;
                }
                *object_scope = 1U;
                *has_object_scope = 1;
            } else if (element->tag.tag_number == 2U && element->value_length == 0U) {
                *object_scope = 2U;
                *has_object_scope = 1;
            }
        } else if (element->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && element->tag.tag_number == 26U) {
            if (domain_id == NULL || domain_id_size == 0U) {
                return 0;
            }
            if (domain_id[0] == '\0') {
                if (!bridge_copy_bytes_as_string(element->value_bytes, element->value_length, domain_id, domain_id_size)) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList domain identifier is too long.", "getNameList domain identifier is too long.");
                    return 0;
                }
            } else if (node_id != NULL && node_id[0] == '\0') {
                if (!bridge_copy_bytes_as_string(element->value_bytes, element->value_length, node_id, node_id_size)) {
                    bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList node identifier is too long.", "getNameList node identifier is too long.");
                    return 0;
                }
            }
            *object_scope = 1U;
            *has_object_scope = 1;
        }
        return 1;
    }

    while (offset < element->value_length) {
        size_t child_consumed_length = 0U;

        unitlab_mms_ber_element_init(&child);
        if (!unitlab_mms_ber_read(&child, &element->value_bytes[offset], element->value_length - offset, &child_consumed_length, &diagnostic->diagnostic)) {
            return 0;
        }
        if (child_consumed_length == 0U) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList request contains a truncated field.", "getNameList request contains a truncated field.");
            return 0;
        }
        if (!bridge_collect_get_name_list_object_scope(&child, object_scope, has_object_scope, domain_id, domain_id_size, node_id, node_id_size, diagnostic)) {
            return 0;
        }
        offset += child_consumed_length;
    }
    return 1;
}

static int bridge_collect_get_name_list_continue_after(const UnitLabMmsBerElement* element, char* continue_after, size_t continue_after_size, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsBerElement child;
    size_t offset = 0U;

    if (element == NULL || continue_after == NULL || continue_after_size == 0U) {
        return 0;
    }

    if (!element->tag.constructed) {
        if (element->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && element->tag.tag_number == 2U) {
            if (!bridge_copy_bytes_as_string(element->value_bytes, element->value_length, continue_after, continue_after_size)) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList continueAfter is too long.", "getNameList continueAfter is too long.");
                return 0;
            }
        }
        return 1;
    }

    while (offset < element->value_length) {
        size_t child_consumed_length = 0U;

        unitlab_mms_ber_element_init(&child);
        if (!unitlab_mms_ber_read(&child, &element->value_bytes[offset], element->value_length - offset, &child_consumed_length, &diagnostic->diagnostic)) {
            return 0;
        }
        if (child_consumed_length == 0U) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList request contains a truncated field.", "getNameList request contains a truncated field.");
            return 0;
        }
        if (!bridge_collect_get_name_list_continue_after(&child, continue_after, continue_after_size, diagnostic)) {
            return 0;
        }
        offset += child_consumed_length;
    }
    return 1;
}

static int bridge_parse_get_name_list_request(const uint8_t* service_bytes, size_t service_length, UnitLabMmsDecodedPdu* decoded_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsBerElement outer_element;
    const uint8_t* fields_bytes = service_bytes;
    size_t fields_length = service_length;
    size_t outer_consumed_length = 0U;
    size_t offset = 0U;
    int has_object_class = 0;
    int has_object_scope = 0;

    if (decoded_pdu == NULL) {
        return 0;
    }
    decoded_pdu->object_class = 0U;
    decoded_pdu->object_scope = 0U;
    decoded_pdu->domain_id[0] = '\0';
    decoded_pdu->node_id[0] = '\0';
    decoded_pdu->continue_after[0] = '\0';

    unitlab_mms_ber_element_init(&outer_element);
    if (unitlab_mms_ber_read(&outer_element, service_bytes, service_length, &outer_consumed_length, &diagnostic->diagnostic)) {
        if (outer_consumed_length == service_length && outer_element.tag.constructed) {
            fields_bytes = outer_element.value_bytes;
            fields_length = outer_element.value_length;
        }
    }

    while (offset < fields_length) {
        UnitLabMmsBerElement child;
        size_t child_consumed_length = 0U;

        unitlab_mms_ber_element_init(&child);
        if (!unitlab_mms_ber_read(&child, &fields_bytes[offset], fields_length - offset, &child_consumed_length, &diagnostic->diagnostic)) {
            return 0;
        }
        if (child_consumed_length == 0U) {
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList request contains a truncated field.", "getNameList request contains a truncated field.");
            return 0;
        }
        if (!bridge_collect_get_name_list_object_class(&child, &decoded_pdu->object_class, &has_object_class, diagnostic)) {
            return 0;
        }
        if (!bridge_collect_get_name_list_object_scope(&child, &decoded_pdu->object_scope, &has_object_scope, decoded_pdu->domain_id, sizeof(decoded_pdu->domain_id), decoded_pdu->node_id, sizeof(decoded_pdu->node_id), diagnostic)) {
            return 0;
        }
        if (!bridge_collect_get_name_list_continue_after(&child, decoded_pdu->continue_after, sizeof(decoded_pdu->continue_after), diagnostic)) {
            return 0;
        }
        offset += child_consumed_length;
    }
    if (!has_object_class || !has_object_scope) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "getNameList request must include objectClass and objectScope.", "getNameList request must include objectClass and objectScope.");
        return 0;
    }
    decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_REQUEST;
    return 1;
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
                if (!bridge_parse_read_request(wire_pdu->service_bytes, wire_pdu->service_length, decoded_pdu, diagnostic)) {
                    return 0;
                }
                decoded_pdu->value_bytes = wire_pdu->service_bytes;
                decoded_pdu->value_length = wire_pdu->service_length;
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_WRITE) {
                if (!bridge_parse_write_request(wire_pdu->service_bytes, wire_pdu->service_length, decoded_pdu, diagnostic)) {
                    return 0;
                }
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_GET_NAME_LIST) {
                if (!bridge_parse_get_name_list_request(wire_pdu->service_bytes, wire_pdu->service_length, decoded_pdu, diagnostic)) {
                    return 0;
                }
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES) {
                if (!bridge_parse_get_variable_access_attributes_request(wire_pdu->service_bytes, wire_pdu->service_length, decoded_pdu, diagnostic)) {
                    return 0;
                }
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES) {
                if (!bridge_parse_get_named_variable_list_attributes_request(wire_pdu->service_bytes, wire_pdu->service_length, decoded_pdu, diagnostic)) {
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
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_GET_NAME_LIST) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_RESPONSE;
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_RESPONSE;
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_RESPONSE;
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
        case UNITLAB_MMS_PDU_CONCLUDE_REQUEST:
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_RELEASE_REQUEST;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
            return 1;
        case UNITLAB_MMS_PDU_CONCLUDE_RESPONSE:
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_RELEASE_RESPONSE;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
            return 1;
        case UNITLAB_MMS_PDU_CONCLUDE_ERROR:
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_ABORT;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_ERROR;
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
