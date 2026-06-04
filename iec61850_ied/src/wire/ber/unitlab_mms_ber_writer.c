#include "unitlab_mms_ber.h"

#include <string.h>

static void ber_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

int unitlab_mms_ber_write(const UnitLabMmsBerElement* element, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t tag_length = 0U;
    size_t length_length = 0U;
    size_t offset = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (element == NULL || buffer == NULL || encoded_length == NULL) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "BER element write requires element, buffer, and encoded_length.");
        return 0;
    }
    if (element->value_length != 0U && element->value_bytes == NULL) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "BER element write requires value bytes when the value length is non-zero.");
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
    if (offset > buffer_length || element->value_length > buffer_length - offset) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "BER element value exceeds the available buffer.");
        return 0;
    }
    if (element->value_length != 0U) {
        memcpy(&buffer[offset], element->value_bytes, element->value_length);
    }
    offset += element->value_length;
    *encoded_length = offset;
    ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
