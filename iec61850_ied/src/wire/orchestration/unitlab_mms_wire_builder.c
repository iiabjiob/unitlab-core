#include "unitlab_mms_wire_builder.h"

#include <stdio.h>
#include <string.h>

#include "wire/acse/unitlab_mms_acse.h"
#include "wire/presentation/unitlab_mms_presentation.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/orchestration/unitlab_mms_association_frame.h"
#include "model/model_plan.h"

#include "unitlab_mms_wire_builder_internal.h"







static int unitlab_mms_extract_calling_tsap(const uint8_t* parameters, size_t parameters_length, uint16_t* calling_tsap)
{
    size_t index = 0U;

    if (calling_tsap == NULL) {
        return 0;
    }
    *calling_tsap = 0x0001U;
    if (parameters == NULL || parameters_length == 0U) {
        return 1;
    }
    while (index + 2U <= parameters_length) {
        uint8_t tag = parameters[index++];
        uint8_t length = parameters[index++];
        size_t value_end = index + (size_t)length;

        if (value_end > parameters_length) {
            return 0;
        }
        if (tag == 0xC1U) {
            if (length == 2U) {
                *calling_tsap = (uint16_t)(((uint16_t)parameters[index] << 8U) | (uint16_t)parameters[index + 1U]);
                return 1;
            }
            if (length == 1U) {
                *calling_tsap = (uint16_t)parameters[index];
                return 1;
            }
            return 0;
        }
        index = value_end;
    }
    return 1;
}

static int unitlab_mms_build_cotp_connect_frame(
    UnitLabMmsCotpTpduKind kind,
    uint16_t destination_reference,
    uint16_t source_reference,
    uint8_t tpdu_class,
    const uint8_t* parameters,
    size_t parameters_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsTransportFrame transport_frame;
    size_t frame_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "COTP connect frame build requires buffer and encoded_length.");
        return 0;
    }
    if (buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "COTP connect frame buffer must be non-zero.");
        return 0;
    }
    if (parameters_length != 0U && parameters == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "COTP connect frame parameters are required when parameters_length is non-zero.");
        return 0;
    }

    unitlab_mms_transport_frame_init(&transport_frame);
    transport_frame.cotp.kind = kind;
    transport_frame.cotp.destination_reference = destination_reference;
    transport_frame.cotp.source_reference = source_reference;
    transport_frame.cotp.tpdu_class = tpdu_class;
    transport_frame.cotp.user_data = parameters;
    transport_frame.cotp.user_data_length = parameters_length;
    if (!unitlab_mms_transport_frame_encode(&transport_frame, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }

    *encoded_length = frame_length;
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_build_cotp_connect_request_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    static const uint8_t parameters[] = { 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };
    return unitlab_mms_build_cotp_connect_frame(
        UNITLAB_MMS_COTP_TPDU_CR,
        0U,
        1U,
        0U,
        parameters,
        sizeof(parameters),
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

int unitlab_mms_build_cotp_connect_response_frame(
    const uint8_t* request_parameters,
    size_t request_parameters_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint16_t calling_tsap = 0x0001U;
    uint8_t parameters[] = { 0xC0U, 0x01U, 0x0AU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };

    if (unitlab_mms_extract_calling_tsap(request_parameters, request_parameters_length, &calling_tsap)) {
        parameters[9] = (uint8_t)((calling_tsap >> 8U) & 0xFFU);
        parameters[10] = (uint8_t)(calling_tsap & 0xFFU);
    }
    return unitlab_mms_build_cotp_connect_frame(
        UNITLAB_MMS_COTP_TPDU_CC,
        1U,
        1U,
        0U,
        parameters,
        sizeof(parameters),
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

int unitlab_mms_build_wire_frame_from_pdu(
    const UnitLabMmsPdu* pdu,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsTransportFrame transport_frame;
    uint8_t presentation_bytes[2048U];
    uint8_t session_bytes[2048U];
    size_t payload_length = 0U;
    size_t presentation_length = 0U;
    size_t session_length = 0U;
    size_t frame_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (pdu == NULL || scratch == NULL || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Wire frame build requires a PDU, scratch buffer, buffer, and encoded_length.");
        return 0;
    }
    if (scratch_length == 0U || buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Wire frame build scratch and buffer must be non-zero.");
        return 0;
    }

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    if (pdu->kind == UNITLAB_MMS_PDU_UNCONFIRMED) {
        if (!unitlab_mms_pdu_encode(pdu, scratch, scratch_length, &payload_length, diagnostic)) {
            return 0;
        }
        presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
        presentation_apdu.context_identifier = 1U;
        presentation_apdu.payload_bytes = scratch;
        presentation_apdu.payload_length = payload_length;
    } else {
        if (!unitlab_mms_pdu_encode(pdu, scratch, scratch_length, &payload_length, diagnostic)) {
            return 0;
        }
        presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
        presentation_apdu.context_identifier = 3U;
        presentation_apdu.payload_bytes = scratch;
        presentation_apdu.payload_length = payload_length;
    }
    if (!unitlab_mms_presentation_encode(&presentation_apdu, presentation_bytes, sizeof(presentation_bytes), &presentation_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_spdu.spdu_bytes = presentation_bytes;
    session_spdu.spdu_length = presentation_length;
    if (!unitlab_mms_session_spdu_encode(&session_spdu, session_bytes, sizeof(session_bytes), &session_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_transport_frame_init(&transport_frame);
    transport_frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    transport_frame.cotp.eot = 1;
    transport_frame.cotp.user_data = session_bytes;
    transport_frame.cotp.user_data_length = session_length;
    if (!unitlab_mms_transport_frame_encode(&transport_frame, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }

    *encoded_length = frame_length;
    wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}



















int unitlab_mms_build_confirmed_response_frame(
    const UnitLabMmsPdu* response_pdu,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (response_pdu == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Confirmed response frame requires a response PDU.");
        return 0;
    }
    if (response_pdu->kind != UNITLAB_MMS_PDU_CONFIRMED_RESPONSE) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Confirmed response frame requires a confirmed response PDU.");
        return 0;
    }
    return unitlab_mms_build_wire_frame_from_pdu(response_pdu, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
}
