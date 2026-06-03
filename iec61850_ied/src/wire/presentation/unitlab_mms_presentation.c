#include "unitlab_mms_presentation.h"

#include <string.h>
#include <stdio.h>

static void presentation_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

void unitlab_mms_presentation_apdu_init(UnitLabMmsPresentationApdu* apdu)
{
    if (apdu == NULL) {
        return;
    }
    memset(apdu, 0, sizeof(*apdu));
    unitlab_mms_ber_tag_init(&apdu->tag);
    apdu->kind = UNITLAB_MMS_PRESENTATION_APDU_NONE;
}

int unitlab_mms_presentation_encode(const UnitLabMmsPresentationApdu* apdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (apdu == NULL || buffer == NULL || encoded_length == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "presentation encode requires apdu, buffer, and encoded_length.");
        return 0;
    }
    if (apdu->kind != UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED && apdu->kind != UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "presentation encode supports simply-encoded-data and fully-encoded-data only.");
        return 0;
    }
    if (apdu->payload_length != 0U && apdu->payload_bytes == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "presentation payload bytes are required when payload length is non-zero.");
        return 0;
    }
    unitlab_mms_ber_element_init(&element);
    if (apdu->kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED) {
        element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
        element.tag.constructed = 0;
        element.tag.tag_number = 0U;
    } else {
        element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
        element.tag.constructed = 1;
        element.tag.tag_number = 1U;
    }
    element.value_bytes = apdu->payload_bytes;
    element.value_length = apdu->payload_length;
    if (!unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_presentation_decode(UnitLabMmsPresentationApdu* apdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (apdu == NULL || buffer == NULL || consumed_length == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "presentation decode requires apdu, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_ber_element_init(&element);
    if (!unitlab_mms_ber_read(&element, buffer, buffer_length, consumed_length, diagnostic)) {
        return 0;
    }
    if (element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION && element.tag.tag_number == 0U && element.tag.constructed == 0) {
        unitlab_mms_presentation_apdu_init(apdu);
        apdu->kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    } else if ((element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && element.tag.tag_number == 16U && element.tag.constructed == 1) || (element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION && element.tag.tag_number == 1U && element.tag.constructed == 1)) {
        unitlab_mms_presentation_apdu_init(apdu);
        apdu->kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    } else {
        if (diagnostic != NULL) {
            snprintf(diagnostic->message, sizeof(diagnostic->message), "presentation user-data tag is unsupported (class=%u constructed=%u tag=%u)", (unsigned)element.tag.tag_class, (unsigned)element.tag.constructed, (unsigned)element.tag.tag_number);
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED;
        }
        return 0;
    }
    apdu->tag = element.tag;
    apdu->payload_bytes = element.value_bytes;
    apdu->payload_length = element.value_length;
    apdu->encoded_length = element.encoded_length;
    presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
