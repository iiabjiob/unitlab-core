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

static int session_read_length_indicator(const uint8_t* buffer, size_t buffer_length, size_t* li_length, size_t* parameter_length)
{
    if (li_length == NULL || parameter_length == NULL) {
        return 0;
    }
    if (buffer_length < 2U) {
        return 0;
    }
    if (buffer[1U] != 0xFFU) {
        *li_length = 1U;
        *parameter_length = (size_t)buffer[1U];
        return 1;
    }
    if (buffer_length < 4U) {
        return 0;
    }
    *li_length = 3U;
    *parameter_length = ((size_t)buffer[2U] << 8U) | (size_t)buffer[3U];
    return 1;
}

static int session_append_bytes(uint8_t* buffer, size_t buffer_length, size_t* offset, const uint8_t* bytes, size_t bytes_length, UnitLabMmsDiagnostic* diagnostic)
{
    if (buffer == NULL || offset == NULL) {
        return 0;
    }
    if (bytes_length != 0U && bytes == NULL) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU bytes are required when length is non-zero.");
        return 0;
    }
    if (*offset > buffer_length || bytes_length > buffer_length - *offset) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "session SPDU buffer is too small.");
        return 0;
    }
    if (bytes_length != 0U) {
        memcpy(&buffer[*offset], bytes, bytes_length);
        *offset += bytes_length;
    }
    return 1;
}

static int session_append_short_tlv(uint8_t tag, const uint8_t* value_bytes, size_t value_length, uint8_t* buffer, size_t buffer_length, size_t* offset, UnitLabMmsDiagnostic* diagnostic)
{
    if (buffer == NULL || offset == NULL) {
        return 0;
    }
    if (*offset > buffer_length || 2U > buffer_length - *offset || value_length > buffer_length - *offset - 2U) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "session SPDU buffer is too small.");
        return 0;
    }
    buffer[(*offset)++] = tag;
    buffer[(*offset)++] = (uint8_t)value_length;
    return session_append_bytes(buffer, buffer_length, offset, value_bytes, value_length, diagnostic);
}

static int session_encode_accept(const UnitLabMmsSessionSpdu* spdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t offset = 0U;
    const uint8_t session_accept_item[] = { 0x13U, 0x01U, 0x00U, 0x16U, 0x01U, 0x02U };
    const uint8_t session_requirement[] = { 0x00U, 0x02U };
    const uint8_t session_selector[] = { 0x00U, 0x01U };

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (spdu == NULL || buffer == NULL || encoded_length == NULL) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session ACCEPT encode requires spdu, buffer, and encoded_length.");
        return 0;
    }
    if (spdu->spdu_length != 0U && spdu->spdu_bytes == NULL) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session ACCEPT user data is required when length is non-zero.");
        return 0;
    }
    if (buffer_length < (2U + 8U + 4U + 4U + 2U) || spdu->spdu_length > buffer_length - (2U + 8U + 4U + 4U + 2U)) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "session ACCEPT buffer is too small.");
        return 0;
    }

    buffer[offset++] = 0x0EU;
    buffer[offset++] = 0x00U;
    if (!session_append_short_tlv(0x05U, session_accept_item, sizeof(session_accept_item), buffer, buffer_length, &offset, diagnostic)) {
        return 0;
    }
    if (!session_append_short_tlv(0x14U, session_requirement, sizeof(session_requirement), buffer, buffer_length, &offset, diagnostic)) {
        return 0;
    }
    if (!session_append_short_tlv(0x34U, session_selector, sizeof(session_selector), buffer, buffer_length, &offset, diagnostic)) {
        return 0;
    }
    if (!session_append_short_tlv(0xC1U, spdu->spdu_bytes, spdu->spdu_length, buffer, buffer_length, &offset, diagnostic)) {
        return 0;
    }
    if (offset < 2U || offset - 2U > 0xFFU) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "session ACCEPT length is too large.");
        return 0;
    }

    buffer[1U] = (uint8_t)(offset - 2U);
    *encoded_length = offset;
    session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
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
    size_t li_length = 0U;
    size_t parameter_length = 0U;
    size_t expected_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (spdu == NULL || buffer == NULL || encoded_length == NULL) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU encode requires spdu, buffer, and encoded_length.");
        return 0;
    }
    if (!session_kind_to_code(spdu->kind, &expected_code)) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported session SPDU kind.");
        return 0;
    }
    if (spdu->kind == UNITLAB_MMS_SESSION_SPDU_ACCEPT) {
        return session_encode_accept(spdu, buffer, buffer_length, encoded_length, diagnostic);
    }
    if (spdu->kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER) {
        if (spdu->spdu_length != 0U && spdu->spdu_bytes == NULL) {
            session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU payload bytes are required when length is non-zero.");
            return 0;
        }
        if (buffer_length < 4U || spdu->spdu_length > buffer_length - 4U) {
            session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "session SPDU buffer is too small.");
            return 0;
        }
        buffer[0U] = expected_code;
        buffer[1U] = 0U;
        buffer[2U] = expected_code;
        buffer[3U] = 0U;
        if (spdu->spdu_length != 0U) {
            memcpy(&buffer[4U], spdu->spdu_bytes, spdu->spdu_length);
        }
        *encoded_length = 4U + spdu->spdu_length;
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    /* Raw SPDU copy: validate the declared kind and the SI/LI length indicator, then preserve the encoded bytes unchanged. */
    if (spdu->spdu_length < 2U) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU length is too small.");
        return 0;
    }
    if (spdu->spdu_bytes == NULL) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU bytes are required when length is non-zero.");
        return 0;
    }
    if (spdu->spdu_bytes[0] != expected_code) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "session SPDU code does not match the declared kind.");
        return 0;
    }
    if (!session_read_length_indicator(spdu->spdu_bytes, spdu->spdu_length, &li_length, &parameter_length)) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "session SPDU length indicator is invalid.");
        return 0;
    }
    expected_length = 1U + li_length + parameter_length;
    if (expected_length != spdu->spdu_length) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "session SPDU length indicator does not match the raw SPDU length.");
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
    size_t li_length = 0U;
    size_t parameter_length = 0U;
    size_t total_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (spdu == NULL || buffer == NULL || consumed_length == NULL) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session SPDU decode requires spdu, buffer, and consumed_length.");
        return 0;
    }
    unitlab_mms_session_spdu_init(spdu);
    if (buffer_length < 2U) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "session SPDU buffer is too small.");
        return 0;
    }
    if (!session_code_to_kind(buffer[0], &kind)) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported session SPDU code.");
        return 0;
    }
    if (kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER) {
        if (buffer_length < 4U || buffer[1U] != 0U || buffer[2U] != buffer[0] || buffer[3U] != 0U) {
            session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "session data transfer SPDU is truncated or invalid.");
            return 0;
        }
        total_length = buffer_length;
        spdu->kind = kind;
        spdu->spdu_bytes = buffer;
        spdu->spdu_length = total_length;
        spdu->encoded_length = total_length;
        spdu->raw_parameter_bytes = &buffer[4U];
        spdu->raw_parameter_length = buffer_length - 4U;
        *consumed_length = total_length;
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (!session_read_length_indicator(buffer, buffer_length, &li_length, &parameter_length)) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "session SPDU length indicator is invalid or truncated.");
        return 0;
    }
    total_length = 1U + li_length + parameter_length;
    if (total_length > buffer_length) {
        session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "session SPDU is truncated.");
        return 0;
    }
    spdu->kind = kind;
    spdu->spdu_bytes = buffer;
    spdu->spdu_length = total_length;
    spdu->encoded_length = total_length;
    spdu->raw_parameter_bytes = &buffer[1U + li_length];
    spdu->raw_parameter_length = parameter_length;
    if (kind == UNITLAB_MMS_SESSION_SPDU_CONNECT || kind == UNITLAB_MMS_SESSION_SPDU_ACCEPT) {
        size_t offset = 0U;
        while (offset + 2U <= spdu->raw_parameter_length) {
            uint8_t parameter_tag = spdu->raw_parameter_bytes[offset];
            size_t parameter_length_bytes = (size_t)spdu->raw_parameter_bytes[offset + 1U];
            if (parameter_length_bytes > spdu->raw_parameter_length - offset - 2U) {
                session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "session SPDU parameter list is truncated.");
                return 0;
            }
            if (parameter_tag == 0xC1U) {
                spdu->raw_parameter_bytes = &spdu->raw_parameter_bytes[offset + 2U];
                spdu->raw_parameter_length = parameter_length_bytes;
                break;
            }
            offset += 2U + parameter_length_bytes;
        }
        if (spdu->raw_parameter_bytes == &buffer[1U + li_length] || spdu->raw_parameter_length == 0U) {
            session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "session SPDU is missing user data bytes.");
            return 0;
        }
    }
    *consumed_length = total_length;
    session_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
