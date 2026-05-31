#include "unitlab_mms_ber.h"

#include <string.h>

void unitlab_mms_ber_element_init(UnitLabMmsBerElement* element)
{
    if (element == NULL) {
        return;
    }
    memset(element, 0, sizeof(*element));
    unitlab_mms_ber_tag_init(&element->tag);
}

int unitlab_mms_ber_read(UnitLabMmsBerElement* element, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerTag tag;
    size_t tag_length = 0U;
    size_t length_length = 0U;
    size_t value_length = 0U;
    size_t offset = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (element == NULL || buffer == NULL || consumed_length == NULL) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            diagnostic->message[0] = '\0';
        }
        return 0;
    }
    unitlab_mms_ber_tag_init(&tag);
    if (!unitlab_mms_ber_tag_decode(&tag, buffer, buffer_length, &tag_length, diagnostic)) {
        return 0;
    }
    offset += tag_length;
    if (!unitlab_mms_ber_length_decode(&value_length, &buffer[offset], buffer_length - offset, &length_length, diagnostic)) {
        return 0;
    }
    offset += length_length;
    if (buffer_length < offset + value_length) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            diagnostic->message[0] = '\0';
        }
        return 0;
    }
    element->tag = tag;
    element->value_bytes = &buffer[offset];
    element->value_length = value_length;
    element->encoded_length = offset + value_length;
    *consumed_length = element->encoded_length;
    if (diagnostic != NULL) {
        diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_OK;
        diagnostic->message[0] = '\0';
    }
    return 1;
}
