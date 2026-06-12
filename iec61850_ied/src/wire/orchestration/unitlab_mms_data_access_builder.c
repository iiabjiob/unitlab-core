#include "unitlab_mms_wire_builder_internal.h"

#include <string.h>

#include "wire/orchestration/unitlab_mms_association_frame.h"

int unitlab_mms_build_read_request_frame(
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement domain_id_element;
    UnitLabMmsBerElement item_id_element;
    UnitLabMmsBerElement object_name_element;
    UnitLabMmsBerElement variable_spec_element;
    UnitLabMmsBerElement list_element;
    UnitLabMmsBerElement list_of_variable_element;
    UnitLabMmsBerElement variable_access_element;
    UnitLabMmsBerElement read_request_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsPdu request_pdu;
    uint8_t domain_id_bytes[64U];
    uint8_t item_id_bytes[128U];
    uint8_t object_name_bytes[192U];
    uint8_t variable_spec_bytes[256U];
    uint8_t list_bytes[320U];
    uint8_t list_of_variable_bytes[384U];
    uint8_t variable_access_bytes[448U];
    uint8_t read_request_bytes[512U];
    uint8_t service_bytes[512U];
    uint8_t request_payload[512U];
    size_t domain_id_length = 0U;
    size_t item_id_length = 0U;
    size_t object_name_length = 0U;
    size_t variable_spec_length = 0U;
    size_t list_length = 0U;
    size_t list_of_variable_length = 0U;
    size_t variable_access_length = 0U;
    size_t read_request_length = 0U;
    size_t service_length = 0U;
    size_t invoke_id_length = 0U;
    size_t request_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (domain_id == NULL || domain_id[0] == '\0' || item_id == NULL || item_id[0] == '\0' || scratch == NULL || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Read request build requires a domain identifier, an item identifier, scratch buffer, buffer, and encoded_length.");
        return 0;
    }
    if (scratch_length == 0U || buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Read request build scratch and buffer must be non-zero.");
        return 0;
    }

    unitlab_mms_ber_element_init(&domain_id_element);
    domain_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    domain_id_element.tag.constructed = 0;
    domain_id_element.tag.tag_number = 26U;
    domain_id_element.value_bytes = (const uint8_t*)domain_id;
    domain_id_element.value_length = strlen(domain_id);
    if (!wire_builder_encode_ber_element(
            domain_id_element.tag.tag_class,
            domain_id_element.tag.constructed,
            domain_id_element.tag.tag_number,
            domain_id_element.value_bytes,
            domain_id_element.value_length,
            domain_id_bytes,
            sizeof(domain_id_bytes),
            &domain_id_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&item_id_element);
    item_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    item_id_element.tag.constructed = 0;
    item_id_element.tag.tag_number = 26U;
    item_id_element.value_bytes = (const uint8_t*)item_id;
    item_id_element.value_length = strlen(item_id);
    if (!wire_builder_encode_ber_element(
            item_id_element.tag.tag_class,
            item_id_element.tag.constructed,
            item_id_element.tag.tag_number,
            item_id_element.value_bytes,
            item_id_element.value_length,
            item_id_bytes,
            sizeof(item_id_bytes),
            &item_id_length,
            diagnostic)) {
        return 0;
    }

    if (domain_id_length + item_id_length > sizeof(object_name_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Read request ObjectName value is too large.");
        return 0;
    }
    memcpy(object_name_bytes, domain_id_bytes, domain_id_length);
    memcpy(object_name_bytes + domain_id_length, item_id_bytes, item_id_length);
    object_name_length = domain_id_length + item_id_length;

    unitlab_mms_ber_element_init(&object_name_element);
    object_name_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    object_name_element.tag.constructed = 1;
    object_name_element.tag.tag_number = 1U;
    object_name_element.value_bytes = object_name_bytes;
    object_name_element.value_length = object_name_length;
    if (!wire_builder_encode_ber_element(
            object_name_element.tag.tag_class,
            object_name_element.tag.constructed,
            object_name_element.tag.tag_number,
            object_name_element.value_bytes,
            object_name_element.value_length,
            variable_spec_bytes,
            sizeof(variable_spec_bytes),
            &variable_spec_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&variable_spec_element);
    variable_spec_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    variable_spec_element.tag.constructed = 1;
    variable_spec_element.tag.tag_number = 0U;
    variable_spec_element.value_bytes = variable_spec_bytes;
    variable_spec_element.value_length = variable_spec_length;
    if (!wire_builder_encode_ber_element(
            variable_spec_element.tag.tag_class,
            variable_spec_element.tag.constructed,
            variable_spec_element.tag.tag_number,
            variable_spec_element.value_bytes,
            variable_spec_element.value_length,
            list_bytes,
            sizeof(list_bytes),
            &list_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&list_element);
    list_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    list_element.tag.constructed = 1;
    list_element.tag.tag_number = 16U;
    list_element.value_bytes = list_bytes;
    list_element.value_length = list_length;
    if (!wire_builder_encode_ber_element(
            list_element.tag.tag_class,
            list_element.tag.constructed,
            list_element.tag.tag_number,
            list_element.value_bytes,
            list_element.value_length,
            list_of_variable_bytes,
            sizeof(list_of_variable_bytes),
            &list_of_variable_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&list_of_variable_element);
    list_of_variable_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    list_of_variable_element.tag.constructed = 1;
    list_of_variable_element.tag.tag_number = 0U;
    list_of_variable_element.value_bytes = list_of_variable_bytes;
    list_of_variable_element.value_length = list_of_variable_length;
    if (!wire_builder_encode_ber_element(
            list_of_variable_element.tag.tag_class,
            list_of_variable_element.tag.constructed,
            list_of_variable_element.tag.tag_number,
            list_of_variable_element.value_bytes,
            list_of_variable_element.value_length,
            variable_access_bytes,
            sizeof(variable_access_bytes),
            &variable_access_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&variable_access_element);
    variable_access_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    variable_access_element.tag.constructed = 1;
    variable_access_element.tag.tag_number = 1U;
    variable_access_element.value_bytes = variable_access_bytes;
    variable_access_element.value_length = variable_access_length;
    if (!wire_builder_encode_ber_element(
            variable_access_element.tag.tag_class,
            variable_access_element.tag.constructed,
            variable_access_element.tag.tag_number,
            variable_access_element.value_bytes,
            variable_access_element.value_length,
            read_request_bytes,
            sizeof(read_request_bytes),
            &read_request_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&read_request_element);
    read_request_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    read_request_element.tag.constructed = 1;
    read_request_element.tag.tag_number = 16U;
    read_request_element.value_bytes = read_request_bytes;
    read_request_element.value_length = read_request_length;
    if (!wire_builder_encode_ber_element(
            read_request_element.tag.tag_class,
            read_request_element.tag.constructed,
            read_request_element.tag.tag_number,
            read_request_element.value_bytes,
            read_request_element.value_length,
            service_bytes,
            sizeof(service_bytes),
            &service_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&service_element);
    service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    service_element.tag.constructed = 1;
    service_element.tag.tag_number = 4U;
    service_element.value_bytes = service_bytes;
    service_element.value_length = service_length;
    if (!wire_builder_encode_ber_element(
            service_element.tag.tag_class,
            service_element.tag.constructed,
            service_element.tag.tag_number,
            service_element.value_bytes,
            service_element.value_length,
            read_request_bytes,
            sizeof(read_request_bytes),
            &service_length,
            diagnostic)) {
        return 0;
    }

    {
        uint8_t invoke_id_raw[5U];
        size_t invoke_id_raw_length = 0U;
        uint32_t value = invoke_id;

        do {
            invoke_id_raw[sizeof(invoke_id_raw) - 1U - invoke_id_raw_length] = (uint8_t)(value & 0xFFU);
            invoke_id_raw_length++;
            value >>= 8U;
        } while (value != 0U && invoke_id_raw_length < sizeof(invoke_id_raw));
        if (invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length] & 0x80U) {
            if (sizeof(invoke_id_raw) == invoke_id_raw_length) {
                wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Read request invokeID encoding is too large.");
                return 0;
            }
            invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length - 1U] = 0x00U;
            invoke_id_raw_length++;
        }
        if (!wire_builder_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
                0,
                2U,
                &invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length],
                invoke_id_raw_length,
                request_payload,
                sizeof(request_payload),
                &invoke_id_length,
                diagnostic)) {
            return 0;
        }
    }
    if (invoke_id_length + service_length > sizeof(request_payload)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Read request payload is too large.");
        return 0;
    }
    memcpy(request_payload + invoke_id_length, read_request_bytes, service_length);
    request_length = invoke_id_length + service_length;

    unitlab_mms_pdu_init(&request_pdu);
    request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    request_pdu.pdu_bytes = request_payload;
    request_pdu.pdu_length = request_length;
    return unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
}

int unitlab_mms_build_write_request_frame(
    const char* domain_id,
    const char* item_id,
    const UnitLabMmsBerElement* data_element,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement domain_id_element;
    UnitLabMmsBerElement item_id_element;
    UnitLabMmsBerElement object_name_element;
    UnitLabMmsBerElement variable_spec_element;
    UnitLabMmsBerElement list_element;
    UnitLabMmsBerElement list_of_variable_element;
    UnitLabMmsBerElement data_wrapper_element;
    UnitLabMmsBerElement write_request_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsPdu request_pdu;
    uint8_t domain_id_bytes[64U];
    uint8_t item_id_bytes[128U];
    uint8_t object_name_bytes[192U];
    uint8_t variable_spec_bytes[256U];
    uint8_t list_bytes[320U];
    uint8_t list_of_variable_bytes[384U];
    uint8_t variable_access_bytes[448U];
    uint8_t data_bytes[128U];
    uint8_t data_wrapper_bytes[192U];
    uint8_t write_request_bytes[512U];
    uint8_t service_bytes[576U];
    uint8_t request_payload[640U];
    size_t domain_id_length = 0U;
    size_t item_id_length = 0U;
    size_t object_name_length = 0U;
    size_t variable_spec_length = 0U;
    size_t list_length = 0U;
    size_t list_of_variable_length = 0U;
    size_t variable_access_length = 0U;
    size_t data_length = 0U;
    size_t data_wrapper_length = 0U;
    size_t write_request_length = 0U;
    size_t service_length = 0U;
    size_t invoke_id_length = 0U;
    size_t request_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (domain_id == NULL || domain_id[0] == '\0' || item_id == NULL || item_id[0] == '\0' || data_element == NULL || scratch == NULL || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Write request build requires a domain identifier, item identifier, data element, scratch buffer, buffer, and encoded_length.");
        return 0;
    }
    if (scratch_length == 0U || buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write request build scratch and buffer must be non-zero.");
        return 0;
    }

    unitlab_mms_ber_element_init(&domain_id_element);
    domain_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    domain_id_element.tag.constructed = 0;
    domain_id_element.tag.tag_number = 26U;
    domain_id_element.value_bytes = (const uint8_t*)domain_id;
    domain_id_element.value_length = strlen(domain_id);
    if (!wire_builder_encode_ber_element(
            domain_id_element.tag.tag_class,
            domain_id_element.tag.constructed,
            domain_id_element.tag.tag_number,
            domain_id_element.value_bytes,
            domain_id_element.value_length,
            domain_id_bytes,
            sizeof(domain_id_bytes),
            &domain_id_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&item_id_element);
    item_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    item_id_element.tag.constructed = 0;
    item_id_element.tag.tag_number = 26U;
    item_id_element.value_bytes = (const uint8_t*)item_id;
    item_id_element.value_length = strlen(item_id);
    if (!wire_builder_encode_ber_element(
            item_id_element.tag.tag_class,
            item_id_element.tag.constructed,
            item_id_element.tag.tag_number,
            item_id_element.value_bytes,
            item_id_element.value_length,
            item_id_bytes,
            sizeof(item_id_bytes),
            &item_id_length,
            diagnostic)) {
        return 0;
    }

    if (domain_id_length + item_id_length > sizeof(object_name_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write request ObjectName value is too large.");
        return 0;
    }
    memcpy(object_name_bytes, domain_id_bytes, domain_id_length);
    memcpy(object_name_bytes + domain_id_length, item_id_bytes, item_id_length);
    object_name_length = domain_id_length + item_id_length;

    unitlab_mms_ber_element_init(&object_name_element);
    object_name_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    object_name_element.tag.constructed = 1;
    object_name_element.tag.tag_number = 1U;
    object_name_element.value_bytes = object_name_bytes;
    object_name_element.value_length = object_name_length;
    if (!wire_builder_encode_ber_element(
            object_name_element.tag.tag_class,
            object_name_element.tag.constructed,
            object_name_element.tag.tag_number,
            object_name_element.value_bytes,
            object_name_element.value_length,
            variable_spec_bytes,
            sizeof(variable_spec_bytes),
            &variable_spec_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&variable_spec_element);
    variable_spec_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    variable_spec_element.tag.constructed = 1;
    variable_spec_element.tag.tag_number = 0U;
    variable_spec_element.value_bytes = variable_spec_bytes;
    variable_spec_element.value_length = variable_spec_length;
    if (!wire_builder_encode_ber_element(
            variable_spec_element.tag.tag_class,
            variable_spec_element.tag.constructed,
            variable_spec_element.tag.tag_number,
            variable_spec_element.value_bytes,
            variable_spec_element.value_length,
            list_bytes,
            sizeof(list_bytes),
            &list_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&list_element);
    list_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    list_element.tag.constructed = 1;
    list_element.tag.tag_number = 16U;
    list_element.value_bytes = list_bytes;
    list_element.value_length = list_length;
    if (!wire_builder_encode_ber_element(
            list_element.tag.tag_class,
            list_element.tag.constructed,
            list_element.tag.tag_number,
            list_element.value_bytes,
            list_element.value_length,
            list_of_variable_bytes,
            sizeof(list_of_variable_bytes),
            &list_of_variable_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&list_of_variable_element);
    list_of_variable_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    list_of_variable_element.tag.constructed = 1;
    list_of_variable_element.tag.tag_number = 0U;
    list_of_variable_element.value_bytes = list_of_variable_bytes;
    list_of_variable_element.value_length = list_of_variable_length;
    if (!wire_builder_encode_ber_element(
            list_of_variable_element.tag.tag_class,
            list_of_variable_element.tag.constructed,
            list_of_variable_element.tag.tag_number,
            list_of_variable_element.value_bytes,
            list_of_variable_element.value_length,
            variable_access_bytes,
            sizeof(variable_access_bytes),
            &variable_access_length,
            diagnostic)) {
        return 0;
    }

    if (!wire_builder_encode_ber_element(
            data_element->tag.tag_class,
            data_element->tag.constructed,
            data_element->tag.tag_number,
            data_element->value_bytes,
            data_element->value_length,
            data_bytes,
            sizeof(data_bytes),
            &data_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&data_wrapper_element);
    data_wrapper_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_wrapper_element.tag.constructed = 1;
    data_wrapper_element.tag.tag_number = 0U;
    data_wrapper_element.value_bytes = data_bytes;
    data_wrapper_element.value_length = data_length;
    if (!wire_builder_encode_ber_element(
            data_wrapper_element.tag.tag_class,
            data_wrapper_element.tag.constructed,
            data_wrapper_element.tag.tag_number,
            data_wrapper_element.value_bytes,
            data_wrapper_element.value_length,
            data_wrapper_bytes,
            sizeof(data_wrapper_bytes),
            &data_wrapper_length,
            diagnostic)) {
        return 0;
    }

    if (variable_access_length + data_wrapper_length > sizeof(write_request_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write request body is too large.");
        return 0;
    }
    memcpy(write_request_bytes, variable_access_bytes, variable_access_length);
    memcpy(write_request_bytes + variable_access_length, data_wrapper_bytes, data_wrapper_length);
    write_request_length = variable_access_length + data_wrapper_length;

    unitlab_mms_ber_element_init(&write_request_element);
    write_request_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    write_request_element.tag.constructed = 1;
    write_request_element.tag.tag_number = 16U;
    write_request_element.value_bytes = write_request_bytes;
    write_request_element.value_length = write_request_length;
    if (!wire_builder_encode_ber_element(
            write_request_element.tag.tag_class,
            write_request_element.tag.constructed,
            write_request_element.tag.tag_number,
            write_request_element.value_bytes,
            write_request_element.value_length,
            service_bytes,
            sizeof(service_bytes),
            &service_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&service_element);
    service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    service_element.tag.constructed = 1;
    service_element.tag.tag_number = 5U;
    service_element.value_bytes = service_bytes;
    service_element.value_length = service_length;
    if (!wire_builder_encode_ber_element(
            service_element.tag.tag_class,
            service_element.tag.constructed,
            service_element.tag.tag_number,
            service_element.value_bytes,
            service_element.value_length,
            write_request_bytes,
            sizeof(write_request_bytes),
            &service_length,
            diagnostic)) {
        return 0;
    }

    {
        uint8_t invoke_id_raw[5U];
        size_t invoke_id_raw_length = 0U;
        uint32_t value = invoke_id;

        do {
            invoke_id_raw[sizeof(invoke_id_raw) - 1U - invoke_id_raw_length] = (uint8_t)(value & 0xFFU);
            invoke_id_raw_length++;
            value >>= 8U;
        } while (value != 0U && invoke_id_raw_length < sizeof(invoke_id_raw));
        if (invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length] & 0x80U) {
            if (sizeof(invoke_id_raw) == invoke_id_raw_length) {
                wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write request invokeID encoding is too large.");
                return 0;
            }
            invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length - 1U] = 0x00U;
            invoke_id_raw_length++;
        }
        if (!wire_builder_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
                0,
                2U,
                &invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length],
                invoke_id_raw_length,
                request_payload,
                sizeof(request_payload),
                &invoke_id_length,
                diagnostic)) {
            return 0;
        }
    }
    if (invoke_id_length + service_length > sizeof(request_payload)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write request payload is too large.");
        return 0;
    }
    memcpy(request_payload + invoke_id_length, write_request_bytes, service_length);
    request_length = invoke_id_length + service_length;

    unitlab_mms_pdu_init(&request_pdu);
    request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    request_pdu.pdu_bytes = request_payload;
    request_pdu.pdu_length = request_length;
    return unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
}

static int unitlab_mms_build_object_name_request_frame(
    uint32_t service_tag_number,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement domain_id_element;
    UnitLabMmsBerElement item_id_element;
    UnitLabMmsBerElement object_name_element;
    UnitLabMmsBerElement name_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsPdu request_pdu;
    uint8_t domain_id_bytes[64U];
    uint8_t item_id_bytes[128U];
    uint8_t object_name_bytes[192U];
    uint8_t name_bytes[256U];
    uint8_t service_bytes[320U];
    uint8_t service_wrapper_bytes[384U];
    uint8_t request_payload[384U];
    size_t domain_id_length = 0U;
    size_t item_id_length = 0U;
    size_t object_name_length = 0U;
    size_t name_length = 0U;
    size_t service_length = 0U;
    size_t service_wrapper_length = 0U;
    size_t invoke_id_length = 0U;
    size_t request_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (item_id == NULL || item_id[0] == '\0' || scratch == NULL || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetVariableAccessAttributes request build requires an item identifier, scratch buffer, buffer, and encoded_length.");
        return 0;
    }
    if (scratch_length == 0U || buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetVariableAccessAttributes request build scratch and buffer must be non-zero.");
        return 0;
    }

    if (domain_id != NULL && domain_id[0] != '\0') {
        unitlab_mms_ber_element_init(&domain_id_element);
        domain_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        domain_id_element.tag.constructed = 0;
        domain_id_element.tag.tag_number = 26U;
        domain_id_element.value_bytes = (const uint8_t*)domain_id;
        domain_id_element.value_length = strlen(domain_id);
        if (!wire_builder_encode_ber_element(
                domain_id_element.tag.tag_class,
                domain_id_element.tag.constructed,
                domain_id_element.tag.tag_number,
                domain_id_element.value_bytes,
                domain_id_element.value_length,
                domain_id_bytes,
                sizeof(domain_id_bytes),
                &domain_id_length,
                diagnostic)) {
            return 0;
        }

        unitlab_mms_ber_element_init(&item_id_element);
        item_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        item_id_element.tag.constructed = 0;
        item_id_element.tag.tag_number = 26U;
        item_id_element.value_bytes = (const uint8_t*)item_id;
        item_id_element.value_length = strlen(item_id);
        if (!wire_builder_encode_ber_element(
                item_id_element.tag.tag_class,
                item_id_element.tag.constructed,
                item_id_element.tag.tag_number,
                item_id_element.value_bytes,
                item_id_element.value_length,
                item_id_bytes,
                sizeof(item_id_bytes),
                &item_id_length,
                diagnostic)) {
            return 0;
        }

        if (domain_id_length + item_id_length > sizeof(object_name_bytes)) {
            wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetVariableAccessAttributes domain-specific ObjectName is too large.");
            return 0;
        }
        memcpy(object_name_bytes, domain_id_bytes, domain_id_length);
        memcpy(object_name_bytes + domain_id_length, item_id_bytes, item_id_length);
        object_name_length = domain_id_length + item_id_length;

        unitlab_mms_ber_element_init(&object_name_element);
        object_name_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        object_name_element.tag.constructed = 1;
        object_name_element.tag.tag_number = 1U;
        object_name_element.value_bytes = object_name_bytes;
        object_name_element.value_length = object_name_length;
    }
    else {
        unitlab_mms_ber_element_init(&object_name_element);
        object_name_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        object_name_element.tag.constructed = 0;
        object_name_element.tag.tag_number = 0U;
        object_name_element.value_bytes = (const uint8_t*)item_id;
        object_name_element.value_length = strlen(item_id);
    }

    if (!wire_builder_encode_ber_element(
            object_name_element.tag.tag_class,
            object_name_element.tag.constructed,
            object_name_element.tag.tag_number,
            object_name_element.value_bytes,
            object_name_element.value_length,
            name_bytes,
            sizeof(name_bytes),
            &name_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&name_element);
    name_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    name_element.tag.constructed = 1;
    name_element.tag.tag_number = 0U;
    name_element.value_bytes = name_bytes;
    name_element.value_length = name_length;
    if (!wire_builder_encode_ber_element(
            name_element.tag.tag_class,
            name_element.tag.constructed,
            name_element.tag.tag_number,
            name_element.value_bytes,
            name_element.value_length,
            service_bytes,
            sizeof(service_bytes),
            &service_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&service_element);
    service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    service_element.tag.constructed = 1;
    service_element.tag.tag_number = service_tag_number;
    service_element.value_bytes = service_bytes;
    service_element.value_length = service_length;
    if (!wire_builder_encode_ber_element(
            service_element.tag.tag_class,
            service_element.tag.constructed,
            service_element.tag.tag_number,
            service_element.value_bytes,
            service_element.value_length,
            service_wrapper_bytes,
            sizeof(service_wrapper_bytes),
            &service_wrapper_length,
            diagnostic)) {
        return 0;
    }

    {
        uint8_t invoke_id_raw[5U];
        size_t invoke_id_raw_length = 0U;
        uint32_t value = invoke_id;

        do {
            invoke_id_raw[sizeof(invoke_id_raw) - 1U - invoke_id_raw_length] = (uint8_t)(value & 0xFFU);
            invoke_id_raw_length++;
            value >>= 8U;
        } while (value != 0U && invoke_id_raw_length < sizeof(invoke_id_raw));
        if (invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length] & 0x80U) {
            if (sizeof(invoke_id_raw) == invoke_id_raw_length) {
                wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetVariableAccessAttributes request invokeID encoding is too large.");
                return 0;
            }
            invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length - 1U] = 0x00U;
            invoke_id_raw_length++;
        }
        if (!wire_builder_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
                0,
                2U,
                &invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length],
                invoke_id_raw_length,
                request_payload,
                sizeof(request_payload),
                &invoke_id_length,
                diagnostic)) {
            return 0;
        }
    }
    if (invoke_id_length + service_wrapper_length > sizeof(request_payload)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetVariableAccessAttributes request payload is too large.");
        return 0;
    }
    memcpy(request_payload + invoke_id_length, service_wrapper_bytes, service_wrapper_length);
    request_length = invoke_id_length + service_wrapper_length;

    unitlab_mms_pdu_init(&request_pdu);
    request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    request_pdu.pdu_bytes = request_payload;
    request_pdu.pdu_length = request_length;
    return unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
}

int unitlab_mms_build_get_variable_access_attributes_request_frame(
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    return unitlab_mms_build_object_name_request_frame(6U, domain_id, item_id, invoke_id, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
}

int unitlab_mms_build_get_named_variable_list_attributes_request_frame(
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement domain_id_element;
    UnitLabMmsBerElement item_id_element;
    UnitLabMmsBerElement object_name_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsPdu request_pdu;
    uint8_t domain_id_bytes[64U];
    uint8_t item_id_bytes[128U];
    uint8_t object_name_bytes[192U];
    uint8_t object_name_encoded_bytes[256U];
    uint8_t service_bytes[256U];
    uint8_t request_payload[320U];
    size_t domain_id_length = 0U;
    size_t item_id_length = 0U;
    size_t object_name_length = 0U;
    size_t service_length = 0U;
    size_t invoke_id_length = 0U;
    size_t request_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (item_id == NULL || item_id[0] == '\0' || scratch == NULL || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNamedVariableListAttributes request build requires an item identifier, scratch buffer, buffer, and encoded_length.");
        return 0;
    }
    if (scratch_length == 0U || buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes request build scratch and buffer must be non-zero.");
        return 0;
    }

    if (domain_id != NULL && domain_id[0] != '\0') {
        unitlab_mms_ber_element_init(&domain_id_element);
        domain_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        domain_id_element.tag.constructed = 0;
        domain_id_element.tag.tag_number = 26U;
        domain_id_element.value_bytes = (const uint8_t*)domain_id;
        domain_id_element.value_length = strlen(domain_id);
        if (!wire_builder_encode_ber_element(domain_id_element.tag.tag_class, domain_id_element.tag.constructed, domain_id_element.tag.tag_number, domain_id_element.value_bytes, domain_id_element.value_length, domain_id_bytes, sizeof(domain_id_bytes), &domain_id_length, diagnostic)) {
            return 0;
        }

        unitlab_mms_ber_element_init(&item_id_element);
        item_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
        item_id_element.tag.constructed = 0;
        item_id_element.tag.tag_number = 26U;
        item_id_element.value_bytes = (const uint8_t*)item_id;
        item_id_element.value_length = strlen(item_id);
        if (!wire_builder_encode_ber_element(item_id_element.tag.tag_class, item_id_element.tag.constructed, item_id_element.tag.tag_number, item_id_element.value_bytes, item_id_element.value_length, item_id_bytes, sizeof(item_id_bytes), &item_id_length, diagnostic)) {
            return 0;
        }
        if (domain_id_length + item_id_length > sizeof(object_name_bytes)) {
            wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes ObjectName is too large.");
            return 0;
        }
        memcpy(object_name_bytes, domain_id_bytes, domain_id_length);
        memcpy(object_name_bytes + domain_id_length, item_id_bytes, item_id_length);
        object_name_length = domain_id_length + item_id_length;

        unitlab_mms_ber_element_init(&object_name_element);
        object_name_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        object_name_element.tag.constructed = 1;
        object_name_element.tag.tag_number = 1U;
        object_name_element.value_bytes = object_name_bytes;
        object_name_element.value_length = object_name_length;
    } else {
        unitlab_mms_ber_element_init(&object_name_element);
        object_name_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        object_name_element.tag.constructed = 0;
        object_name_element.tag.tag_number = 0U;
        object_name_element.value_bytes = (const uint8_t*)item_id;
        object_name_element.value_length = strlen(item_id);
    }

    if (!wire_builder_encode_ber_element(object_name_element.tag.tag_class, object_name_element.tag.constructed, object_name_element.tag.tag_number, object_name_element.value_bytes, object_name_element.value_length, object_name_encoded_bytes, sizeof(object_name_encoded_bytes), &object_name_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&service_element);
    service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    service_element.tag.constructed = 1;
    service_element.tag.tag_number = 12U;
    service_element.value_bytes = object_name_encoded_bytes;
    service_element.value_length = object_name_length;
    if (!wire_builder_encode_ber_element(service_element.tag.tag_class, service_element.tag.constructed, service_element.tag.tag_number, service_element.value_bytes, service_element.value_length, service_bytes, sizeof(service_bytes), &service_length, diagnostic)) {
        return 0;
    }

    {
        uint8_t invoke_id_raw[5U];
        size_t invoke_id_raw_length = 0U;
        uint32_t value = invoke_id;

        do {
            invoke_id_raw[sizeof(invoke_id_raw) - 1U - invoke_id_raw_length] = (uint8_t)(value & 0xFFU);
            invoke_id_raw_length++;
            value >>= 8U;
        } while (value != 0U && invoke_id_raw_length < sizeof(invoke_id_raw));
        if (invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length] & 0x80U) {
            if (sizeof(invoke_id_raw) == invoke_id_raw_length) {
                wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes request invokeID encoding is too large.");
                return 0;
            }
            invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length - 1U] = 0x00U;
            invoke_id_raw_length++;
        }
        if (!wire_builder_encode_ber_element(UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U, &invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length], invoke_id_raw_length, request_payload, sizeof(request_payload), &invoke_id_length, diagnostic)) {
            return 0;
        }
    }
    if (invoke_id_length + service_length > sizeof(request_payload)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes request payload is too large.");
        return 0;
    }
    memcpy(request_payload + invoke_id_length, service_bytes, service_length);
    request_length = invoke_id_length + service_length;

    unitlab_mms_pdu_init(&request_pdu);
    request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    request_pdu.pdu_bytes = request_payload;
    request_pdu.pdu_length = request_length;
    return unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
}

int unitlab_mms_build_get_name_list_request_frame(
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
    const char* continue_after,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    return unitlab_mms_build_get_name_list_request_frame_ex(
        object_class,
        object_scope,
        domain_id,
        NULL,
        continue_after,
        invoke_id,
        scratch,
        scratch_length,
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

int unitlab_mms_build_get_name_list_request_frame_ex(
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
    const char* node_id,
    const char* continue_after,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement object_class_element;
    UnitLabMmsBerElement object_scope_element;
    UnitLabMmsBerElement object_scope_outer_element;
    UnitLabMmsBerElement continue_after_element;
    UnitLabMmsBerElement sequence_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsPdu request_pdu;
    uint8_t object_class_value[5U];
    uint8_t object_class_bytes[16U];
    uint8_t object_class_value_bytes[32U];
    uint8_t object_scope_value_bytes[128U];
    uint8_t object_scope_bytes[128U];
    uint8_t continue_after_bytes[64U];
    uint8_t sequence_value_bytes[192U];
    uint8_t sequence_encoded_bytes[256U];
    uint8_t service_bytes[256U];
    uint8_t request_payload[320U];
    size_t object_class_value_length = 0U;
    size_t object_class_length = 0U;
    size_t object_class_value_length_outer = 0U;
    size_t object_scope_value_length = 0U;
    size_t object_scope_length = 0U;
    size_t continue_after_length = 0U;
    size_t sequence_length = 0U;
    size_t service_length = 0U;
    size_t invoke_id_length = 0U;
    size_t request_length = 0U;
    size_t sequence_value_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (scratch == NULL || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList request build requires scratch buffer, buffer, and encoded_length.");
        return 0;
    }
    if (scratch_length == 0U || buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList request build scratch and buffer must be non-zero.");
        return 0;
    }
    if (object_scope == 1U && (domain_id == NULL || domain_id[0] == '\0')) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList domain scope requires a domain identifier.");
        return 0;
    }
    if (object_scope != 0U && object_scope != 1U && object_scope != 2U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "GetNameList object scope is unsupported.");
        return 0;
    }

    if (!wire_builder_encode_unsigned_integer(object_class, object_class_value, sizeof(object_class_value), &object_class_value_length, diagnostic)) {
        return 0;
    }
    unitlab_mms_ber_element_init(&object_class_element);
    object_class_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    object_class_element.tag.constructed = 0;
    object_class_element.tag.tag_number = 0U;
    object_class_element.value_bytes = object_class_value;
    object_class_element.value_length = object_class_value_length;
    if (!wire_builder_encode_ber_element(
            object_class_element.tag.tag_class,
            object_class_element.tag.constructed,
            object_class_element.tag.tag_number,
            object_class_element.value_bytes,
            object_class_element.value_length,
            object_class_bytes,
            sizeof(object_class_bytes),
            &object_class_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&object_scope_element);
    object_scope_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    object_scope_element.tag.constructed = 0;
    object_scope_element.tag.tag_number = object_scope;
    object_scope_element.value_bytes = object_scope == 1U ? (const uint8_t*)domain_id : NULL;
    object_scope_element.value_length = object_scope == 1U ? strlen(domain_id) : 0U;
    if (!wire_builder_encode_ber_element(
            object_scope_element.tag.tag_class,
            object_scope_element.tag.constructed,
            object_scope_element.tag.tag_number,
            object_scope_element.value_bytes,
            object_scope_element.value_length,
            object_scope_bytes,
            sizeof(object_scope_bytes),
            &object_scope_value_length,
            diagnostic)) {
        return 0;
    }
    if (node_id != NULL && node_id[0] != '\0') {
        size_t node_id_length = 0U;
        if (!wire_builder_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
                0,
                26U,
                (const uint8_t*)node_id,
                strlen(node_id),
                &object_scope_bytes[object_scope_value_length],
                sizeof(object_scope_bytes) - object_scope_value_length,
                &node_id_length,
                diagnostic)) {
            return 0;
        }
        object_scope_value_length += node_id_length;
    }

    unitlab_mms_ber_element_init(&object_scope_outer_element);
    object_scope_outer_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    object_scope_outer_element.tag.constructed = 1;
    object_scope_outer_element.tag.tag_number = 1U;
    object_scope_outer_element.value_bytes = object_scope_bytes;
    object_scope_outer_element.value_length = object_scope_value_length;
    if (!wire_builder_encode_ber_element(
            object_scope_outer_element.tag.tag_class,
            object_scope_outer_element.tag.constructed,
            object_scope_outer_element.tag.tag_number,
            object_scope_outer_element.value_bytes,
            object_scope_outer_element.value_length,
            object_scope_value_bytes,
            sizeof(object_scope_value_bytes),
            &object_scope_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&object_class_element);
    object_class_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    object_class_element.tag.constructed = 0;
    object_class_element.tag.tag_number = 2U;
    object_class_element.value_bytes = object_class_value;
    object_class_element.value_length = object_class_value_length;
    if (!wire_builder_encode_ber_element(
            object_class_element.tag.tag_class,
            object_class_element.tag.constructed,
            object_class_element.tag.tag_number,
            object_class_element.value_bytes,
            object_class_element.value_length,
            object_class_bytes,
            sizeof(object_class_bytes),
            &object_class_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&object_class_element);
    object_class_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    object_class_element.tag.constructed = 1;
    object_class_element.tag.tag_number = 0U;
    object_class_element.value_bytes = object_class_bytes;
    object_class_element.value_length = object_class_length;
    if (!wire_builder_encode_ber_element(
            object_class_element.tag.tag_class,
            object_class_element.tag.constructed,
            object_class_element.tag.tag_number,
            object_class_element.value_bytes,
            object_class_element.value_length,
            object_class_value_bytes,
            sizeof(object_class_value_bytes),
            &object_class_value_length_outer,
            diagnostic)) {
        return 0;
    }

    if (sequence_value_length + object_class_value_length_outer > sizeof(sequence_value_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList request sequence is too large.");
        return 0;
    }
    memcpy(sequence_value_bytes + sequence_value_length, object_class_value_bytes, object_class_value_length_outer);
    sequence_value_length += object_class_value_length_outer;
    if (sequence_value_length + object_scope_length > sizeof(sequence_value_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList request sequence is too large.");
        return 0;
    }
    memcpy(sequence_value_bytes + sequence_value_length, object_scope_value_bytes, object_scope_length);
    sequence_value_length += object_scope_length;

    if (continue_after != NULL && continue_after[0] != '\0') {
        unitlab_mms_ber_element_init(&continue_after_element);
        continue_after_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        continue_after_element.tag.constructed = 0;
        continue_after_element.tag.tag_number = 2U;
        continue_after_element.value_bytes = (const uint8_t*)continue_after;
        continue_after_element.value_length = strlen(continue_after);
        if (!wire_builder_encode_ber_element(
                continue_after_element.tag.tag_class,
                continue_after_element.tag.constructed,
                continue_after_element.tag.tag_number,
                continue_after_element.value_bytes,
                continue_after_element.value_length,
                continue_after_bytes,
                sizeof(continue_after_bytes),
                &continue_after_length,
                diagnostic)) {
            return 0;
        }
        if (sequence_value_length + continue_after_length > sizeof(sequence_value_bytes)) {
            wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList request sequence is too large.");
            return 0;
        }
        memcpy(sequence_value_bytes + sequence_value_length, continue_after_bytes, continue_after_length);
        sequence_value_length += continue_after_length;
    }

    unitlab_mms_ber_element_init(&sequence_element);
    sequence_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    sequence_element.tag.constructed = 1;
    sequence_element.tag.tag_number = 16U;
    sequence_element.value_bytes = sequence_value_bytes;
    sequence_element.value_length = sequence_value_length;
    if (!wire_builder_encode_ber_element(
            sequence_element.tag.tag_class,
            sequence_element.tag.constructed,
            sequence_element.tag.tag_number,
            sequence_element.value_bytes,
            sequence_element.value_length,
            sequence_encoded_bytes,
            sizeof(sequence_encoded_bytes),
            &sequence_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&service_element);
    service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    service_element.tag.constructed = 1;
    service_element.tag.tag_number = 1U;
    service_element.value_bytes = sequence_encoded_bytes;
    service_element.value_length = sequence_length;
    if (!wire_builder_encode_ber_element(
            service_element.tag.tag_class,
            service_element.tag.constructed,
            service_element.tag.tag_number,
            service_element.value_bytes,
            service_element.value_length,
            service_bytes,
            sizeof(service_bytes),
            &service_length,
            diagnostic)) {
        return 0;
    }

    {
        uint8_t invoke_id_raw[5U];
        size_t invoke_id_raw_length = 0U;
        uint32_t value = invoke_id;

        do {
            invoke_id_raw[sizeof(invoke_id_raw) - 1U - invoke_id_raw_length] = (uint8_t)(value & 0xFFU);
            invoke_id_raw_length++;
            value >>= 8U;
        } while (value != 0U && invoke_id_raw_length < sizeof(invoke_id_raw));
        if (invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length] & 0x80U) {
            if (sizeof(invoke_id_raw) == invoke_id_raw_length) {
                wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList request invokeID encoding is too large.");
                return 0;
            }
            invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length - 1U] = 0x00U;
            invoke_id_raw_length++;
        }
        if (!wire_builder_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
                0,
                2U,
                &invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length],
                invoke_id_raw_length,
                request_payload,
                sizeof(request_payload),
                &invoke_id_length,
                diagnostic)) {
            return 0;
        }
    }
    if (invoke_id_length + service_length > sizeof(request_payload)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList request payload is too large.");
        return 0;
    }
    memcpy(&request_payload[invoke_id_length], service_bytes, service_length);
    request_length = invoke_id_length + service_length;

    unitlab_mms_pdu_init(&request_pdu);
    request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    request_pdu.pdu_bytes = request_payload;
    request_pdu.pdu_length = request_length;
    return unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
}

