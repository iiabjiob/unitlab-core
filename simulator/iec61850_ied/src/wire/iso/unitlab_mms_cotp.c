#include "unitlab_mms_cotp.h"

#include <string.h>

static void cotp_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
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

static void cotp_write_u16(uint8_t* buffer, uint16_t value)
{
    buffer[0] = (uint8_t)((value >> 8U) & 0xFFU);
    buffer[1] = (uint8_t)(value & 0xFFU);
}

static uint16_t cotp_read_u16(const uint8_t* buffer)
{
    return (uint16_t)(((uint16_t)buffer[0] << 8U) | (uint16_t)buffer[1]);
}

void unitlab_mms_cotp_tpdu_init(UnitLabMmsCotpTpdu* tpdu)
{
    if (tpdu == NULL) {
        return;
    }
    memset(tpdu, 0, sizeof(*tpdu));
    tpdu->kind = UNITLAB_MMS_COTP_TPDU_NONE;
}

int unitlab_mms_cotp_encode(const UnitLabMmsCotpTpdu* tpdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t index = 0U;
    size_t li_index;
    size_t tpdu_length;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (tpdu == NULL || buffer == NULL || encoded_length == NULL) {
        cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "COTP encode requires tpdu, buffer, and encoded_length.");
        return 0;
    }
    if (tpdu->user_data_length != 0U && tpdu->user_data == NULL) {
        cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "COTP user data is required when user_data_length is non-zero.");
        return 0;
    }
    if (buffer_length < 2U) {
        cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "COTP buffer is too small.");
        return 0;
    }
    li_index = index++;
    switch (tpdu->kind) {
        case UNITLAB_MMS_COTP_TPDU_CR:
        case UNITLAB_MMS_COTP_TPDU_CC:
            if (buffer_length < index + 6U + tpdu->user_data_length) {
                cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "COTP buffer is too small.");
                return 0;
            }
            buffer[index++] = (tpdu->kind == UNITLAB_MMS_COTP_TPDU_CR) ? 0xE0U : 0xD0U;
            cotp_write_u16(&buffer[index], tpdu->destination_reference);
            index += 2U;
            cotp_write_u16(&buffer[index], tpdu->source_reference);
            index += 2U;
            buffer[index++] = tpdu->tpdu_class;
            if (tpdu->user_data_length != 0U) {
                memcpy(&buffer[index], tpdu->user_data, tpdu->user_data_length);
                index += tpdu->user_data_length;
            }
            break;
        case UNITLAB_MMS_COTP_TPDU_DR:
            if (buffer_length < index + 5U) {
                cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "COTP buffer is too small.");
                return 0;
            }
            buffer[index++] = 0x80U;
            cotp_write_u16(&buffer[index], tpdu->destination_reference);
            index += 2U;
            cotp_write_u16(&buffer[index], tpdu->source_reference);
            index += 2U;
            buffer[index++] = tpdu->reason;
            break;
        case UNITLAB_MMS_COTP_TPDU_DT:
            if (buffer_length < index + 2U + tpdu->user_data_length) {
                cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "COTP buffer is too small.");
                return 0;
            }
            buffer[index++] = 0xF0U;
            buffer[index++] = (uint8_t)(tpdu->eot ? 0x80U : 0x00U);
            if (tpdu->user_data_length != 0U) {
                memcpy(&buffer[index], tpdu->user_data, tpdu->user_data_length);
                index += tpdu->user_data_length;
            }
            break;
        default:
            cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported COTP TPDU kind.");
            return 0;
    }
    tpdu_length = index - 1U;
    buffer[li_index] = (uint8_t)tpdu_length;
    *encoded_length = index;
    cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_cotp_decode(UnitLabMmsCotpTpdu* tpdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t index = 0U;
    size_t total_length;
    size_t tpdu_length;
    uint8_t code;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (tpdu == NULL || buffer == NULL || consumed_length == NULL) {
        cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "COTP decode requires tpdu, buffer, and consumed_length.");
        return 0;
    }
    if (buffer_length < 2U) {
        cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "COTP buffer is too small.");
        return 0;
    }
    tpdu_length = buffer[index++];
    total_length = tpdu_length + 1U;
    if (tpdu_length < 1U || total_length > buffer_length) {
        cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "COTP length indicator is invalid.");
        return 0;
    }
    code = buffer[index++];
    unitlab_mms_cotp_tpdu_init(tpdu);
    tpdu->payload_bytes = &buffer[1];
    tpdu->payload_length = total_length - 1U;
    switch (code) {
        case 0xE0U:
            if (total_length < index + 5U) {
                cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "COTP CR TPDU is truncated.");
                return 0;
            }
            tpdu->kind = UNITLAB_MMS_COTP_TPDU_CR;
            tpdu->destination_reference = cotp_read_u16(&buffer[index]);
            index += 2U;
            tpdu->source_reference = cotp_read_u16(&buffer[index]);
            index += 2U;
            tpdu->tpdu_class = buffer[index++];
            break;
        case 0xD0U:
            if (total_length < index + 5U) {
                cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "COTP CC TPDU is truncated.");
                return 0;
            }
            tpdu->kind = UNITLAB_MMS_COTP_TPDU_CC;
            tpdu->destination_reference = cotp_read_u16(&buffer[index]);
            index += 2U;
            tpdu->source_reference = cotp_read_u16(&buffer[index]);
            index += 2U;
            tpdu->tpdu_class = buffer[index++];
            break;
        case 0x80U:
            if (total_length < index + 5U) {
                cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "COTP DR TPDU is truncated.");
                return 0;
            }
            tpdu->kind = UNITLAB_MMS_COTP_TPDU_DR;
            tpdu->destination_reference = cotp_read_u16(&buffer[index]);
            index += 2U;
            tpdu->source_reference = cotp_read_u16(&buffer[index]);
            index += 2U;
            tpdu->reason = buffer[index++];
            break;
        case 0xF0U:
            if (total_length < index + 1U) {
                cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "COTP DT TPDU is truncated.");
                return 0;
            }
            tpdu->kind = UNITLAB_MMS_COTP_TPDU_DT;
            tpdu->eot = (buffer[index++] & 0x80U) != 0U;
            break;
        default:
            cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported COTP TPDU kind.");
            return 0;
    }
    if (total_length > index) {
        tpdu->user_data = &buffer[index];
        tpdu->user_data_length = total_length - index;
    }
    tpdu->encoded_length = total_length;
    *consumed_length = total_length;
    cotp_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
