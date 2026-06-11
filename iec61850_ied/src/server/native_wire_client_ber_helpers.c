#include "native_wire_client_ber_helpers.h"

#include <string.h>

int unitlab_native_client_bytes_are_printable_ascii(const uint8_t* bytes, size_t length)
{
    if (bytes == NULL) {
        return 0;
    }
    for (size_t index = 0U; index < length; index++) {
        if (bytes[index] < 0x20U || bytes[index] > 0x7EU) {
            return 0;
        }
    }
    return 1;
}

int unitlab_native_client_decode_object_name_domain_item(
    const UnitLabMmsBerElement* object_name,
    char* domain,
    size_t domain_size,
    char* item,
    size_t item_size)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement child;
    size_t consumed = 0U;
    size_t offset = 0U;

    if (object_name == NULL || domain == NULL || item == NULL || domain_size == 0U || item_size == 0U) {
        return 0;
    }
    domain[0] = '\0';
    item[0] = '\0';
    unitlab_mms_diagnostic_clear(&diagnostic);
    if (object_name->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && !object_name->tag.constructed && object_name->tag.tag_number == 0U) {
        size_t item_length = object_name->value_length < item_size - 1U ? object_name->value_length : item_size - 1U;
        memcpy(item, object_name->value_bytes, item_length);
        item[item_length] = '\0';
        return 1;
    }
    if (!(object_name->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && object_name->tag.constructed && object_name->tag.tag_number == 1U)) {
        return 0;
    }
    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, object_name->value_bytes, object_name->value_length, &consumed, &diagnostic)
        || child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
        || child.tag.tag_number != 26U) {
        return 0;
    }
    {
        size_t domain_length = child.value_length < domain_size - 1U ? child.value_length : domain_size - 1U;
        memcpy(domain, child.value_bytes, domain_length);
        domain[domain_length] = '\0';
    }
    offset += consumed;
    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, &object_name->value_bytes[offset], object_name->value_length - offset, &consumed, &diagnostic)
        || child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
        || child.tag.tag_number != 26U) {
        return 0;
    }
    {
        size_t item_length = child.value_length < item_size - 1U ? child.value_length : item_size - 1U;
        memcpy(item, child.value_bytes, item_length);
        item[item_length] = '\0';
    }
    return 1;
}
