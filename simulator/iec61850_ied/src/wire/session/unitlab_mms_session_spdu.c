#include "unitlab_mms_session_spdu.h"

#include <string.h>

static void session_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

static int session_kind_to_code(UnitLabMmsSessionSpduKind kind, uint8_t* code)
{
    if (code == NULL) {
        return 0;
    }
    switch (kind) {
        case UNITLAB_MMS_SESSION_SPDU_CONNECT:
            *code = 13U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_CONNECT_DATA_OVERFLOW:
            *code = 15U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_OVERFLOW_ACCEPT:
            *code = 16U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_ACCEPT:
            *code = 14U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_REFUSE:
            *code = 12U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_FINISH:
            *code = 9U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_DISCONNECT:
            *code = 10U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_ABORT:
            *code = 25U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_ABORT_ACCEPT:
            *code = 26U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER:
            *code = 1U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_EXPEDITED_DATA:
            *code = 5U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_TYPED_DATA:
            *code = 33U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA:
            *code = 61U;
            return 1;
        case UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA_ACK:
            *code = 62U;
            return 1;
        default:
            return 0;
    }
}

static int session_code_to_kind(uint8_t code, UnitLabMmsSessionSpduKind* kind)
{
    if (kind == NULL) {
        return 0;
    }
    switch (code) {
        case 13U:
            *kind = UNITLAB_MMS_SESSION_SPDU_CONNECT;
            return 1;
        case 15U:
            *kind = UNITLAB_MMS_SESSION_SPDU_CONNECT_DATA_OVERFLOW;
            return 1;
        case 16U:
            *kind = UNITLAB_MMS_SESSION_SPDU_OVERFLOW_ACCEPT;
            return 1;
        case 14U:
            *kind = UNITLAB_MMS_SESSION_SPDU_ACCEPT;
            return 1;
        case 12U:
            *kind = UNITLAB_MMS_SESSION_SPDU_REFUSE;
            return 1;
        case 9U:
            *kind = UNITLAB_MMS_SESSION_SPDU_FINISH;
            return 1;
        case 10U:
            *kind = UNITLAB_MMS_SESSION_SPDU_DISCONNECT;
            return 1;
        case 25U:
            *kind = UNITLAB_MMS_SESSION_SPDU_ABORT;
            return 1;
        case 26U:
            *kind = UNITLAB_MMS_SESSION_SPDU_ABORT_ACCEPT;
            return 1;
        case 1U:
            *kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
            return 1;
        case 5U:
            *kind = UNITLAB_MMS_SESSION_SPDU_EXPEDITED_DATA;
            return 1;
        case 33U:
            *kind = UNITLAB_MMS_SESSION_SPDU_TYPED_DATA;
            return 1;
        case 61U:
            *kind = UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA;
            return 1;
        case 62U:
            *kind = UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA_ACK;
            return 1;
        default:
            return 0;
    }
}

void unitlab_mms_session_spdu_init(UnitLabMmsSessionSpdu* spdu)
{
    if (spdu == NULL) {
        return;
    }
    memset(spdu, 0, sizeof(*spdu));
    spdu->kind = UNITLAB_MMS_SESSION_SPDU_NONE;
}

int unitlab_mms_session_spdu_encode(const UnitLabMmsSessionSpdu* spdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t expected_code = 0U;

    /* Raw full-buffer wrapper: validate the declared kind against the first SPDU byte, then copy the raw SPDU bytes unchanged. */

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (spdu == NULL || buffer == NULL || encoded_length == NULL) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU encode requires spdu, buffer, and encoded_length.");
        return 0;
    }
    if (spdu->spdu_length != 0U && spdu->spdu_bytes == NULL) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU bytes are required when length is non-zero.");
        return 0;
    }
    if (spdu->spdu_length == 0U) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU length must be non-zero.");
        return 0;
    }
    if (!session_kind_to_code(spdu->kind, &expected_code)) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported session SPDU kind.");
        return 0;
    }
    if (spdu->spdu_bytes[0] != expected_code) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "session SPDU code does not match the declared kind.");
        return 0;
    }
    if (spdu->spdu_length > buffer_length) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "session SPDU buffer is too small.");
        return 0;
    }
    memcpy(buffer, spdu->spdu_bytes, spdu->spdu_length);
    *encoded_length = spdu->spdu_length;
    session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_session_spdu_decode(UnitLabMmsSessionSpdu* spdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsSessionSpduKind kind;
    size_t raw_parameter_length = 0U;


    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (spdu == NULL || buffer == NULL || consumed_length == NULL) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU decode requires spdu, buffer, and consumed_length.");
        return 0;
    }
    if (buffer_length == 0U) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "session SPDU buffer is empty.");
        return 0;
    }
    if (!session_code_to_kind(buffer[0], &kind)) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported session SPDU code.");
        return 0;
    }
    unitlab_mms_session_spdu_init(spdu);
    spdu->kind = kind;
    spdu->spdu_bytes = buffer;
    spdu->spdu_length = buffer_length;
    spdu->encoded_length = buffer_length;
    if (buffer_length > 1U) {
        raw_parameter_length = buffer_length - 1U;
        spdu->raw_parameter_bytes = &buffer[1];
        spdu->raw_parameter_length = raw_parameter_length;
    }
    *consumed_length = buffer_length;
    session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
