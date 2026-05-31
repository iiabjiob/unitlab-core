#include "unitlab_mms_ber.h"

#include <string.h>

int unitlab_mms_ber_write(const UnitLabMmsBerElement* element, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t tag_length = 0U;
    size_t length_length = 0U;
    size_t offset = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (element == NULL || buffer == NULL || encoded_length == NULL) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            diagnostic->message[0] = '\0';
        }
        return 0;
    }
    if (!unitlab_mms_ber_tag_encode(&element->tag, buffer, buffer_length, &tag_length, diagnostic)) {
        return 0;
    }
    offset += tag_length;
    if (!unitlab_mms_ber_length_encode(element->value_length, &buffer[offset], buffer_length - offset, &length_length, diagnostic)) {
        return 0;
    }
    offset += length_length;
    if (buffer_length < offset + element->value_length) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            diagnostic->message[0] = '\0';
        }
        return 0;
    }
    if (element->value_length != 0U && element->value_bytes != NULL) {
        memcpy(&buffer[offset], element->value_bytes, element->value_length);
    }
    offset += element->value_length;
    *encoded_length = offset;
    if (diagnostic != NULL) {
        diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_OK;
        diagnostic->message[0] = '\0';
    }
    return 1;
}
