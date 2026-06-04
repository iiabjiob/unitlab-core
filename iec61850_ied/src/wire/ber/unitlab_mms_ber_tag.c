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

void unitlab_mms_ber_tag_init(UnitLabMmsBerTag* tag)
{
    if (tag == NULL) {
        return;
    }
    memset(tag, 0, sizeof(*tag));
}

static size_t ber_tag_number_encoded_length(uint32_t tag_number)
{
    size_t count = 1U;
    while (tag_number >= 128U) {
        count++;
        tag_number >>= 7U;
    }
    return count;
}

int unitlab_mms_ber_tag_encode(const UnitLabMmsBerTag* tag, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t index = 0U;
    uint32_t tag_number;
    uint8_t first_octet;
    size_t encoded_tag_number_length;
    uint8_t tag_octets[5U];
    size_t tag_octet_count;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (tag == NULL || buffer == NULL || encoded_length == NULL) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "BER tag encode requires tag, buffer, and encoded_length.");
        return 0;
    }
    if (tag->tag_class > UNITLAB_MMS_BER_TAG_CLASS_PRIVATE) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER tag class is invalid.");
        return 0;
    }
    tag_number = tag->tag_number;
    encoded_tag_number_length = ber_tag_number_encoded_length(tag_number);
    if (tag->tag_number < 31U) {
        if (buffer_length < 1U) {
            ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "BER tag buffer is too small.");
            return 0;
        }
        first_octet = (uint8_t)(((uint8_t)tag->tag_class << 6U) | ((tag->constructed != 0) ? 0x20U : 0x00U) | (uint8_t)tag->tag_number);
        buffer[0] = first_octet;
        *encoded_length = 1U;
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (buffer_length < 1U + encoded_tag_number_length) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "BER tag buffer is too small.");
        return 0;
    }
    first_octet = (uint8_t)(((uint8_t)tag->tag_class << 6U) | ((tag->constructed != 0) ? 0x20U : 0x00U) | 0x1FU);
    buffer[index++] = first_octet;
    tag_octet_count = encoded_tag_number_length;
    for (size_t i = 0U; i < tag_octet_count; i++) {
        size_t shift = (tag_octet_count - 1U - i) * 7U;
        tag_octets[i] = (uint8_t)((tag->tag_number >> shift) & 0x7FU);
        if (i != tag_octet_count - 1U) {
            tag_octets[i] |= 0x80U;
        }
    }
    for (size_t i = 0U; i < tag_octet_count; i++) {
        buffer[index++] = tag_octets[i];
    }
    *encoded_length = index;
    ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_ber_tag_decode(UnitLabMmsBerTag* tag, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t index = 0U;
    uint8_t first_octet;
    uint32_t tag_number = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (tag == NULL || buffer == NULL || consumed_length == NULL) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "BER tag decode requires tag, buffer, and consumed_length.");
        return 0;
    }
    if (buffer_length < 1U) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "BER tag buffer is too small.");
        return 0;
    }
    first_octet = buffer[index++];
    tag->tag_class = (UnitLabMmsBerTagClass)((first_octet >> 6U) & 0x03U);
    tag->constructed = (first_octet & 0x20U) != 0U;
    if ((first_octet & 0x1FU) != 0x1FU) {
        tag->tag_number = (uint32_t)(first_octet & 0x1FU);
        *consumed_length = index;
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    do {
        uint8_t octet;
        uint32_t low_bits;

        if (index >= buffer_length) {
            ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "BER tag uses truncated high-tag-number encoding.");
            return 0;
        }
        octet = buffer[index++];
        low_bits = (uint32_t)(octet & 0x7FU);
        if (buffer_length > 2U && buffer[1U] == 0x80U) {
            ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER high-tag-number uses a non-minimal leading zero.");
            return 0;
        }
        if (tag_number > ((UINT32_MAX - low_bits) >> 7U)) {
            ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER tag number overflow.");
            return 0;
        }
        tag_number = (tag_number << 7U) | low_bits;
        if ((octet & 0x80U) == 0U) {
            break;
        }
    } while (1);
    if (tag_number < 31U) {
        ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "BER tag uses non-minimal high-tag-number form.");
        return 0;
    }
    tag->tag_number = tag_number;
    *consumed_length = index;
    ber_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
