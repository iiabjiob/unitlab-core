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

int unitlab_mms_ber_length_encode(size_t value_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t octet_count = 0U;
    size_t tmp = value_length;
    uint8_t length_octets[sizeof(size_t)];

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "BER length encode requires buffer and encoded_length.");
        return 0;
    }
    if (value_length <= 127U) {
        if (buffer_length < 1U) {
            ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "BER length buffer is too small.");
            return 0;
        }
        buffer[0] = (uint8_t)value_length;
        *encoded_length = 1U;
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    while (tmp != 0U) {
        length_octets[octet_count++] = (uint8_t)(tmp & 0xFFU);
        tmp >>= 8U;
    }
    if (octet_count == 0U || octet_count > sizeof(size_t)) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER length is not encodable.");
        return 0;
    }
    if (buffer_length < 1U + octet_count) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "BER length buffer is too small.");
        return 0;
    }
    buffer[0] = (uint8_t)(0x80U | (uint8_t)octet_count);
    for (size_t i = 0U; i < octet_count; i++) {
        buffer[1U + i] = length_octets[octet_count - 1U - i];
    }
    *encoded_length = 1U + octet_count;
    ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_ber_length_decode(size_t* value_length, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t index = 0U;
    size_t length = 0U;
    size_t octet_count;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (value_length == NULL || buffer == NULL || consumed_length == NULL) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "BER length decode requires value_length, buffer, and consumed_length.");
        return 0;
    }
    if (buffer_length < 1U) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "BER length buffer is too small.");
        return 0;
    }
    if ((buffer[index] & 0x80U) == 0U) {
        *value_length = (size_t)(buffer[index] & 0x7FU);
        *consumed_length = 1U;
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    octet_count = (size_t)(buffer[index] & 0x7FU);
    if (octet_count == 0U) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER indefinite length is unsupported.");
        return 0;
    }
    if (octet_count > sizeof(size_t)) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER length octet count is unsupported.");
        return 0;
    }
    if (buffer_length < 1U + octet_count) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "BER length buffer is too small.");
        return 0;
    }
    if (buffer[1U] == 0U) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER length must use the shortest definite form.");
        return 0;
    }
    for (size_t i = 0U; i < octet_count; i++) {
        if (length > (SIZE_MAX >> 8U)) {
            ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER length overflow.");
            return 0;
        }
        length = (length << 8U) | (size_t)buffer[1U + i];
    }
    if (length <= 127U) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER length must use short form for values up to 127.");
        return 0;
    }
    *value_length = length;
    *consumed_length = 1U + octet_count;
    ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
