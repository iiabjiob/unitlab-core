#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../src/wire/iso/unitlab_mms_tpkt.h"
#include "../../src/wire/iso/unitlab_mms_cotp.h"
#include "../../src/wire/session/unitlab_mms_session_spdu.h"
#include "../../src/wire/presentation/unitlab_mms_presentation.h"
#include "../../src/wire/acse/unitlab_mms_acse.h"
#include "../../src/wire/mms/unitlab_mms_pdu.h"
#include "../../src/protocols/mms/unitlab_mms_types.h"
#include "../../src/wire/transport/unitlab_mms_transport_frame.h"

static void assert_bytes_equal(const uint8_t* actual, const uint8_t* expected, size_t length)
{
    assert(length == 0U || actual != NULL);
    assert(length == 0U || expected != NULL);
    assert(memcmp(actual, expected, length) == 0);
}

static void ladder_01_tpkt_raw_payload(void)
{
    uint8_t frame[32];
    const uint8_t payload[] = { 0x11U, 0x22U, 0x33U, 0x44U };
    const uint8_t* decoded_payload = NULL;
    size_t frame_length = 0U;
    size_t payload_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_wrap(payload, sizeof(payload), frame, sizeof(frame), &frame_length, &diagnostic) == 1);
    assert(frame_length == sizeof(payload) + 4U);
    assert(unitlab_mms_tpkt_unwrap(frame, frame_length, &decoded_payload, &payload_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(payload_length == sizeof(payload));
    assert_bytes_equal(decoded_payload, payload, sizeof(payload));
}

static void ladder_02_transport_frame_cotp_dt(void)
{
    uint8_t buffer[64];
    UnitLabMmsTransportFrame frame;
    UnitLabMmsTransportFrame decoded_frame;
    const uint8_t payload[] = { 0xA1U, 0xB2U, 0xC3U };
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = payload;
    frame.cotp.user_data_length = sizeof(payload);
    assert(unitlab_mms_transport_frame_encode(&frame, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length >= 4U);
    assert(buffer[0] == 0x03U);
    assert(buffer[1] == 0x00U);
    assert((((size_t)buffer[2] << 8U) | (size_t)buffer[3]) == encoded_length);

    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_frame.tpkt.version == 3U);
    assert(decoded_frame.tpkt.reserved == 0U);
    assert(decoded_frame.tpkt.length == encoded_length);
    assert(decoded_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_frame.cotp.eot == 1);
    assert(decoded_frame.cotp.user_data_length == sizeof(payload));
    assert_bytes_equal(decoded_frame.cotp.user_data, payload, sizeof(payload));
}

static void ladder_03_transport_session_dt(void)
{
    uint8_t session_buffer[64];
    uint8_t frame_buffer[96];
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsSessionSpdu decoded_session_spdu;
    UnitLabMmsTransportFrame frame;
    UnitLabMmsTransportFrame decoded_frame;
    const uint8_t payload[] = { 0x01U, 0x02U, 0x03U };
    size_t session_length = 0U;
    size_t frame_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_spdu.spdu_bytes = payload;
    session_spdu.spdu_length = sizeof(payload);
    assert(unitlab_mms_session_spdu_encode(&session_spdu, session_buffer, sizeof(session_buffer), &session_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = session_buffer;
    frame.cotp.user_data_length = session_length;
    assert(unitlab_mms_transport_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, frame_buffer, frame_length, &transport_consumed_length, &diagnostic) == 1);
    assert(transport_consumed_length == frame_length);
    assert(decoded_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_frame.cotp.eot == 1);
    assert(decoded_frame.cotp.user_data_length == session_length);
    assert_bytes_equal(decoded_frame.cotp.user_data, session_buffer, session_length);

    unitlab_mms_session_spdu_init(&decoded_session_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_session_spdu, decoded_frame.cotp.user_data, decoded_frame.cotp.user_data_length, &session_consumed_length, &diagnostic) == 1);
    assert(session_consumed_length == session_length);
    assert(decoded_session_spdu.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_session_spdu.raw_parameter_length == sizeof(payload));
    assert_bytes_equal(decoded_session_spdu.raw_parameter_bytes, payload, sizeof(payload));
}

static void ladder_04_transport_session_presentation_simply_encoded(void)
{
    uint8_t presentation_buffer[64];
    uint8_t session_buffer[96];
    uint8_t frame_buffer[128];
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsPresentationApdu decoded_presentation_apdu;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsSessionSpdu decoded_session_spdu;
    UnitLabMmsTransportFrame frame;
    UnitLabMmsTransportFrame decoded_frame;
    const uint8_t payload[] = { 0x10U, 0x20U, 0x30U };
    size_t presentation_length = 0U;
    size_t session_length = 0U;
    size_t frame_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    presentation_apdu.payload_bytes = payload;
    presentation_apdu.payload_length = sizeof(payload);
    assert(unitlab_mms_presentation_encode(&presentation_apdu, presentation_buffer, sizeof(presentation_buffer), &presentation_length, &diagnostic) == 1);

    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_spdu.spdu_bytes = presentation_buffer;
    session_spdu.spdu_length = presentation_length;
    assert(unitlab_mms_session_spdu_encode(&session_spdu, session_buffer, sizeof(session_buffer), &session_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = session_buffer;
    frame.cotp.user_data_length = session_length;
    assert(unitlab_mms_transport_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, frame_buffer, frame_length, &transport_consumed_length, &diagnostic) == 1);
    assert(transport_consumed_length == frame_length);
    assert_bytes_equal(decoded_frame.cotp.user_data, session_buffer, session_length);

    unitlab_mms_session_spdu_init(&decoded_session_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_session_spdu, decoded_frame.cotp.user_data, decoded_frame.cotp.user_data_length, &session_consumed_length, &diagnostic) == 1);
    assert(session_consumed_length == session_length);
    assert(decoded_session_spdu.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_session_spdu.raw_parameter_length == presentation_length);
    assert_bytes_equal(decoded_session_spdu.raw_parameter_bytes, presentation_buffer, presentation_length);

    unitlab_mms_presentation_apdu_init(&decoded_presentation_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_presentation_apdu, decoded_session_spdu.raw_parameter_bytes, decoded_session_spdu.raw_parameter_length, &presentation_consumed_length, &diagnostic) == 1);
    assert(presentation_consumed_length == presentation_length);
    assert(decoded_presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_presentation_apdu.payload_length == sizeof(payload));
    assert_bytes_equal(decoded_presentation_apdu.payload_bytes, payload, sizeof(payload));
}

static void ladder_05_transport_session_presentation_fully_encoded(void)
{
    uint8_t presentation_buffer[64];
    uint8_t session_buffer[96];
    uint8_t frame_buffer[128];
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsPresentationApdu decoded_presentation_apdu;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsSessionSpdu decoded_session_spdu;
    UnitLabMmsTransportFrame frame;
    UnitLabMmsTransportFrame decoded_frame;
    const uint8_t payload[] = { 0x30U, 0x02U, 0x01U, 0x01U };
    size_t presentation_length = 0U;
    size_t session_length = 0U;
    size_t frame_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    presentation_apdu.context_identifier = 7U;
    presentation_apdu.payload_bytes = payload;
    presentation_apdu.payload_length = sizeof(payload);
    assert(unitlab_mms_presentation_encode(&presentation_apdu, presentation_buffer, sizeof(presentation_buffer), &presentation_length, &diagnostic) == 1);

    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_spdu.spdu_bytes = presentation_buffer;
    session_spdu.spdu_length = presentation_length;
    assert(unitlab_mms_session_spdu_encode(&session_spdu, session_buffer, sizeof(session_buffer), &session_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = session_buffer;
    frame.cotp.user_data_length = session_length;
    assert(unitlab_mms_transport_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, frame_buffer, frame_length, &transport_consumed_length, &diagnostic) == 1);
    assert(transport_consumed_length == frame_length);
    assert_bytes_equal(decoded_frame.cotp.user_data, session_buffer, session_length);

    unitlab_mms_session_spdu_init(&decoded_session_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_session_spdu, decoded_frame.cotp.user_data, decoded_frame.cotp.user_data_length, &session_consumed_length, &diagnostic) == 1);
    assert(session_consumed_length == session_length);
    assert(decoded_session_spdu.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_session_spdu.raw_parameter_length == presentation_length);
    assert_bytes_equal(decoded_session_spdu.raw_parameter_bytes, presentation_buffer, presentation_length);

    unitlab_mms_presentation_apdu_init(&decoded_presentation_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_presentation_apdu, decoded_session_spdu.raw_parameter_bytes, decoded_session_spdu.raw_parameter_length, &presentation_consumed_length, &diagnostic) == 1);
    assert(presentation_consumed_length == presentation_length);
    assert(decoded_presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(decoded_presentation_apdu.context_identifier == 7U);
    assert(decoded_presentation_apdu.payload_length == sizeof(payload));
    assert_bytes_equal(decoded_presentation_apdu.payload_bytes, payload, sizeof(payload));
}

static void ladder_06_transport_session_presentation_acse(void)
{
    uint8_t acse_buffer[96];
    uint8_t presentation_buffer[128];
    uint8_t session_buffer[160];
    uint8_t frame_buffer[192];
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsAcseApdu decoded_acse_apdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsPresentationApdu decoded_presentation_apdu;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsSessionSpdu decoded_session_spdu;
    UnitLabMmsTransportFrame frame;
    UnitLabMmsTransportFrame decoded_frame;
    const uint8_t acse_payload[] = { 0x80U, 0x01U, 0x11U, 0xA2U, 0x03U, 0x80U, 0x01U, 0x22U };
    size_t acse_length = 0U;
    size_t presentation_length = 0U;
    size_t session_length = 0U;
    size_t frame_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    size_t acse_consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    acse_apdu.apdu_bytes = acse_payload;
    acse_apdu.apdu_length = sizeof(acse_payload);
    assert(unitlab_mms_acse_encode(&acse_apdu, acse_buffer, sizeof(acse_buffer), &acse_length, &diagnostic) == 1);

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    presentation_apdu.payload_bytes = acse_buffer;
    presentation_apdu.payload_length = acse_length;
    assert(unitlab_mms_presentation_encode(&presentation_apdu, presentation_buffer, sizeof(presentation_buffer), &presentation_length, &diagnostic) == 1);

    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_spdu.spdu_bytes = presentation_buffer;
    session_spdu.spdu_length = presentation_length;
    assert(unitlab_mms_session_spdu_encode(&session_spdu, session_buffer, sizeof(session_buffer), &session_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = session_buffer;
    frame.cotp.user_data_length = session_length;
    assert(unitlab_mms_transport_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, frame_buffer, frame_length, &transport_consumed_length, &diagnostic) == 1);
    assert(transport_consumed_length == frame_length);
    assert_bytes_equal(decoded_frame.cotp.user_data, session_buffer, session_length);

    unitlab_mms_session_spdu_init(&decoded_session_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_session_spdu, decoded_frame.cotp.user_data, decoded_frame.cotp.user_data_length, &session_consumed_length, &diagnostic) == 1);
    assert(session_consumed_length == session_length);
    assert(decoded_session_spdu.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_session_spdu.raw_parameter_length == presentation_length);
    assert_bytes_equal(decoded_session_spdu.raw_parameter_bytes, presentation_buffer, presentation_length);

    unitlab_mms_presentation_apdu_init(&decoded_presentation_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_presentation_apdu, decoded_session_spdu.raw_parameter_bytes, decoded_session_spdu.raw_parameter_length, &presentation_consumed_length, &diagnostic) == 1);
    assert(presentation_consumed_length == presentation_length);
    assert(decoded_presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_presentation_apdu.payload_length == acse_length);
    assert_bytes_equal(decoded_presentation_apdu.payload_bytes, acse_buffer, acse_length);

    unitlab_mms_acse_apdu_init(&decoded_acse_apdu);
    assert(unitlab_mms_acse_decode(&decoded_acse_apdu, decoded_presentation_apdu.payload_bytes, decoded_presentation_apdu.payload_length, &acse_consumed_length, &diagnostic) == 1);
    assert(acse_consumed_length == acse_length);
    assert(decoded_acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARQ);
    assert(decoded_acse_apdu.apdu_length == sizeof(acse_payload));
    assert_bytes_equal(decoded_acse_apdu.apdu_bytes, acse_payload, sizeof(acse_payload));
}

static void ladder_07_transport_session_presentation_acse_mms_pdu(void)
{
    uint8_t mms_buffer[64];
    uint8_t acse_buffer[96];
    uint8_t presentation_buffer[128];
    uint8_t session_buffer[160];
    uint8_t frame_buffer[192];
    UnitLabMmsPdu mms_pdu;
    UnitLabMmsPdu decoded_mms_pdu;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsAcseApdu decoded_acse_apdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsPresentationApdu decoded_presentation_apdu;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsSessionSpdu decoded_session_spdu;
    UnitLabMmsTransportFrame frame;
    UnitLabMmsTransportFrame decoded_frame;
    const uint8_t mms_payload[] = { 0x01U, 0x23U, 0x45U, 0x67U };
    size_t mms_length = 0U;
    size_t acse_length = 0U;
    size_t presentation_length = 0U;
    size_t session_length = 0U;
    size_t frame_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    size_t acse_consumed_length = 0U;
    size_t mms_consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&mms_pdu);
    mms_pdu.kind = UNITLAB_MMS_PDU_INITIATE_REQUEST;
    mms_pdu.pdu_bytes = mms_payload;
    mms_pdu.pdu_length = sizeof(mms_payload);
    assert(unitlab_mms_pdu_encode(&mms_pdu, mms_buffer, sizeof(mms_buffer), &mms_length, &diagnostic) == 1);

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    acse_apdu.apdu_bytes = mms_buffer;
    acse_apdu.apdu_length = mms_length;
    assert(unitlab_mms_acse_encode(&acse_apdu, acse_buffer, sizeof(acse_buffer), &acse_length, &diagnostic) == 1);

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    presentation_apdu.payload_bytes = acse_buffer;
    presentation_apdu.payload_length = acse_length;
    assert(unitlab_mms_presentation_encode(&presentation_apdu, presentation_buffer, sizeof(presentation_buffer), &presentation_length, &diagnostic) == 1);

    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_spdu.spdu_bytes = presentation_buffer;
    session_spdu.spdu_length = presentation_length;
    assert(unitlab_mms_session_spdu_encode(&session_spdu, session_buffer, sizeof(session_buffer), &session_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = session_buffer;
    frame.cotp.user_data_length = session_length;
    assert(unitlab_mms_transport_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, frame_buffer, frame_length, &transport_consumed_length, &diagnostic) == 1);
    assert(transport_consumed_length == frame_length);
    assert_bytes_equal(decoded_frame.cotp.user_data, session_buffer, session_length);

    unitlab_mms_session_spdu_init(&decoded_session_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_session_spdu, decoded_frame.cotp.user_data, decoded_frame.cotp.user_data_length, &session_consumed_length, &diagnostic) == 1);
    assert(session_consumed_length == session_length);
    assert(decoded_session_spdu.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_session_spdu.raw_parameter_length == presentation_length);
    assert_bytes_equal(decoded_session_spdu.raw_parameter_bytes, presentation_buffer, presentation_length);

    unitlab_mms_presentation_apdu_init(&decoded_presentation_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_presentation_apdu, decoded_session_spdu.raw_parameter_bytes, decoded_session_spdu.raw_parameter_length, &presentation_consumed_length, &diagnostic) == 1);
    assert(presentation_consumed_length == presentation_length);
    assert(decoded_presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_presentation_apdu.payload_length == acse_length);
    assert_bytes_equal(decoded_presentation_apdu.payload_bytes, acse_buffer, acse_length);

    unitlab_mms_acse_apdu_init(&decoded_acse_apdu);
    assert(unitlab_mms_acse_decode(&decoded_acse_apdu, decoded_presentation_apdu.payload_bytes, decoded_presentation_apdu.payload_length, &acse_consumed_length, &diagnostic) == 1);
    assert(acse_consumed_length == acse_length);
    assert(decoded_acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARQ);
    assert(decoded_acse_apdu.apdu_length == mms_length);
    assert_bytes_equal(decoded_acse_apdu.apdu_bytes, mms_buffer, mms_length);

    unitlab_mms_pdu_init(&decoded_mms_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_mms_pdu, decoded_acse_apdu.apdu_bytes, decoded_acse_apdu.apdu_length, &mms_consumed_length, &diagnostic) == 1);
    assert(mms_consumed_length == mms_length);
    assert(decoded_mms_pdu.kind == UNITLAB_MMS_PDU_INITIATE_REQUEST);
    assert(decoded_mms_pdu.pdu_length == sizeof(mms_payload));
    assert_bytes_equal(decoded_mms_pdu.pdu_bytes, mms_payload, sizeof(mms_payload));
}

int main(void)
{
    ladder_01_tpkt_raw_payload();
    ladder_02_transport_frame_cotp_dt();
    ladder_03_transport_session_dt();
    ladder_04_transport_session_presentation_simply_encoded();
    ladder_05_transport_session_presentation_fully_encoded();
    ladder_06_transport_session_presentation_acse();
    ladder_07_transport_session_presentation_acse_mms_pdu();
    return 0;
}
