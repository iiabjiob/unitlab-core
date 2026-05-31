#include "unitlab_mms_presentation.h"

#include <string.h>

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
    if (apdu->kind != UNITLAB_MMS_PRESENTATION_APDU_RAW) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "presentation encode supports raw APDU only.");
        return 0;
    }
    if (apdu->payload_length != 0U && apdu->payload_bytes == NULL) {
        presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "presentation payload bytes are required when payload length is non-zero.");
        return 0;
    }
    unitlab_mms_ber_element_init(&element);
    element.tag = apdu->tag;
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
    unitlab_mms_presentation_apdu_init(apdu);
    apdu->kind = UNITLAB_MMS_PRESENTATION_APDU_RAW;
    apdu->tag = element.tag;
    apdu->payload_bytes = element.value_bytes;
    apdu->payload_length = element.value_length;
    apdu->encoded_length = element.encoded_length;
    presentation_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
