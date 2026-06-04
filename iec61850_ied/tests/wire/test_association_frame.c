#include "../src/wire/acse/unitlab_mms_acse.h"
#include "../src/wire/session/unitlab_mms_session_spdu.h"
#include "../src/wire/ber/unitlab_mms_ber.h"
#include "../src/wire/presentation/unitlab_mms_presentation.h"
#include "../src/wire/transport/unitlab_mms_transport_frame.h"
#include "../src/wire/orchestration/unitlab_mms_association_frame.h"
#include "../src/wire/orchestration/unitlab_mms_wire_builder.h"
#include "../src/wire/mms/unitlab_mms_pdu.h"
#include "../src/wire/iso/unitlab_mms_cotp.h"
#include "../src/wire/iso/unitlab_mms_tpkt.h"
#include "protocols/mms/unitlab_mms_core.h"
#include "protocols/mms/unitlab_mms_wire_semantic_bridge.h"
#include <assert.h>
#include <string.h>

static void test_association_frame_decode_roundtrip(void)
{
    uint8_t acse_buffer[64];
    uint8_t presentation_buffer[96];
    uint8_t frame_buffer[128];
    UnitLabMmsPdu mms_pdu;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsAssociationFrame fixture;
    UnitLabMmsAssociationFrame decoded_fixture;
    size_t mms_length = 0U;
    size_t acse_length = 0U;
    size_t presentation_length = 0U;
    size_t frame_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t mms_payload[7] = { 0x02U, 0x01U, 0x05U, 0x80U, 0x01U, 0xAAU, 0x00U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&mms_pdu);
    mms_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    mms_pdu.pdu_bytes = mms_payload;
    mms_pdu.pdu_length = sizeof(mms_payload);
    assert(unitlab_mms_pdu_encode(&mms_pdu, acse_buffer, sizeof(acse_buffer), &mms_length, &diagnostic) == 1);

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    acse_apdu.apdu_bytes = acse_buffer;
    acse_apdu.apdu_length = mms_length;
    assert(unitlab_mms_acse_encode(&acse_apdu, acse_buffer, sizeof(acse_buffer), &acse_length, &diagnostic) == 1);

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    presentation_apdu.payload_bytes = acse_buffer;
    presentation_apdu.payload_length = acse_length;
    presentation_apdu.context_identifier = 3U;
    assert(unitlab_mms_presentation_encode(&presentation_apdu, presentation_buffer, sizeof(presentation_buffer), &presentation_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&fixture);
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    fixture.presentation.payload_bytes = acse_buffer;
    fixture.presentation.payload_length = acse_length;
    assert(unitlab_mms_association_frame_encode(&fixture, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&decoded_fixture);
    assert(unitlab_mms_association_frame_decode(&decoded_fixture, frame_buffer, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_fixture.transport.tpkt.version == 3U);
    assert(decoded_fixture.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_fixture.session.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_fixture.session.raw_parameter_length == presentation_length);
    assert(memcmp(decoded_fixture.session.raw_parameter_bytes, presentation_buffer, presentation_length) == 0);
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_fixture.presentation.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION);
    assert(decoded_fixture.presentation.tag.constructed == 0);
    assert(decoded_fixture.presentation.tag.tag_number == 0U);
    assert(decoded_fixture.presentation.payload_length == acse_length);
    assert(memcmp(decoded_fixture.presentation.payload_bytes, acse_buffer, acse_length) == 0);
}
static void test_association_frame_fully_encoded_roundtrip(void)
{
    uint8_t acse_buffer[64];
    uint8_t presentation_buffer[96];
    uint8_t frame_buffer[128];
    UnitLabMmsPdu mms_pdu;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsAssociationFrame fixture;
    UnitLabMmsAssociationFrame decoded_fixture;
    size_t mms_length = 0U;
    size_t acse_length = 0U;
    size_t presentation_length = 0U;
    size_t frame_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t mms_payload[7] = { 0x02U, 0x01U, 0x05U, 0x80U, 0x01U, 0xAAU, 0x00U };
    const uint8_t fully_encoded_payload[4] = { 0x30U, 0x02U, 0x01U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&mms_pdu);
    mms_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    mms_pdu.pdu_bytes = mms_payload;
    mms_pdu.pdu_length = sizeof(mms_payload);
    assert(unitlab_mms_pdu_encode(&mms_pdu, acse_buffer, sizeof(acse_buffer), &mms_length, &diagnostic) == 1);

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    acse_apdu.apdu_bytes = acse_buffer;
    acse_apdu.apdu_length = mms_length;
    assert(unitlab_mms_acse_encode(&acse_apdu, acse_buffer, sizeof(acse_buffer), &acse_length, &diagnostic) == 1);

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    presentation_apdu.payload_bytes = fully_encoded_payload;
    presentation_apdu.payload_length = sizeof(fully_encoded_payload);
    assert(unitlab_mms_presentation_encode(&presentation_apdu, presentation_buffer, sizeof(presentation_buffer), &presentation_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&fixture);
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    fixture.presentation.payload_bytes = fully_encoded_payload;
    fixture.presentation.payload_length = sizeof(fully_encoded_payload);
    assert(unitlab_mms_association_frame_encode(&fixture, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&decoded_fixture);
    assert(unitlab_mms_association_frame_decode(&decoded_fixture, frame_buffer, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_fixture.transport.tpkt.version == 3U);
    assert(decoded_fixture.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_fixture.session.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_fixture.session.raw_parameter_length == presentation_length);
    assert(memcmp(decoded_fixture.session.raw_parameter_bytes, presentation_buffer, presentation_length) == 0);
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(decoded_fixture.presentation.context_identifier == 1U);
    assert(decoded_fixture.presentation.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION);
    assert(decoded_fixture.presentation.tag.constructed == 1);
    assert(decoded_fixture.presentation.tag.tag_number == 1U);
    assert(decoded_fixture.presentation.payload_length == sizeof(fully_encoded_payload));
    assert(memcmp(decoded_fixture.presentation.payload_bytes, fully_encoded_payload, sizeof(fully_encoded_payload)) == 0);
}
static void test_association_frame_encode_roundtrip(void)
{
    uint8_t acse_buffer[64];
    uint8_t presentation_buffer[96];
    uint8_t frame_buffer[128];
    UnitLabMmsPdu mms_pdu;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsAssociationFrame fixture;
    UnitLabMmsAssociationFrame decoded_fixture;
    size_t mms_length = 0U;
    size_t acse_length = 0U;
    size_t presentation_length = 0U;
    size_t frame_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t mms_payload[7] = { 0x02U, 0x01U, 0x05U, 0x80U, 0x01U, 0xAAU, 0x00U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&mms_pdu);
    mms_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    mms_pdu.pdu_bytes = mms_payload;
    mms_pdu.pdu_length = sizeof(mms_payload);
    assert(unitlab_mms_pdu_encode(&mms_pdu, acse_buffer, sizeof(acse_buffer), &mms_length, &diagnostic) == 1);

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    acse_apdu.apdu_bytes = acse_buffer;
    acse_apdu.apdu_length = mms_length;
    assert(unitlab_mms_acse_encode(&acse_apdu, acse_buffer, sizeof(acse_buffer), &acse_length, &diagnostic) == 1);

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    presentation_apdu.payload_bytes = acse_buffer;
    presentation_apdu.payload_length = acse_length;
    assert(unitlab_mms_presentation_encode(&presentation_apdu, presentation_buffer, sizeof(presentation_buffer), &presentation_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&fixture);
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    fixture.presentation.payload_bytes = acse_buffer;
    fixture.presentation.payload_length = acse_length;
    assert(unitlab_mms_association_frame_encode(&fixture, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&decoded_fixture);
    assert(unitlab_mms_association_frame_decode(&decoded_fixture, frame_buffer, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_fixture.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_fixture.session.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_fixture.session.raw_parameter_length == presentation_length);
    assert(memcmp(decoded_fixture.session.raw_parameter_bytes, presentation_buffer, presentation_length) == 0);
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_fixture.presentation.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION);
    assert(decoded_fixture.presentation.tag.constructed == 0);
    assert(decoded_fixture.presentation.tag.tag_number == 0U);
    assert(decoded_fixture.presentation.payload_length == acse_length);
    assert(memcmp(decoded_fixture.presentation.payload_bytes, acse_buffer, acse_length) == 0);
}
static void test_association_frame_simply_encoded_layers_roundtrip(void)
{
    uint8_t acse_buffer[64];
    uint8_t presentation_buffer[96];
    uint8_t session_buffer[128];
    uint8_t frame_buffer[160];
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsAcseApdu decoded_acse_apdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsPresentationApdu decoded_presentation_apdu;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsSessionSpdu decoded_session_spdu;
    UnitLabMmsTransportFrame decoded_transport_frame;
    UnitLabMmsAssociationFrame frame;
    UnitLabMmsAssociationFrame decoded_frame;
    const uint8_t acse_payload[] = { 0x80U, 0x01U, 0x11U };
    size_t acse_length = 0U;
    size_t presentation_length = 0U;
    size_t session_length = 0U;
    size_t frame_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    size_t acse_consumed_length = 0U;
    size_t decoded_consumed_length = 123U;
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

    unitlab_mms_association_frame_init(&frame);
    unitlab_mms_transport_frame_init(&decoded_transport_frame);
    frame.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.transport.cotp.eot = 1;
    frame.transport.cotp.user_data = session_buffer;
    frame.transport.cotp.user_data_length = session_length;
    frame.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    frame.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    frame.presentation.payload_bytes = acse_buffer;
    frame.presentation.payload_length = acse_length;
    assert(unitlab_mms_association_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&decoded_frame);
    assert(unitlab_mms_association_frame_decode(&decoded_frame, frame_buffer, frame_length, &decoded_consumed_length, &diagnostic) == 1);
    assert(decoded_consumed_length == frame_length);
    assert(decoded_frame.transport.tpkt.version == 3U);
    assert(decoded_frame.transport.tpkt.reserved == 0U);
    assert(decoded_frame.transport.tpkt.length == frame_length);
    assert(decoded_frame.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_frame.transport.cotp.eot == 1);
    assert(decoded_frame.session.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_frame.presentation.payload_length == acse_length);
    assert(memcmp(decoded_frame.presentation.payload_bytes, acse_buffer, acse_length) == 0);

    assert(unitlab_mms_transport_frame_decode(&decoded_transport_frame, frame_buffer, frame_length, &transport_consumed_length, &diagnostic) == 1);
    assert(transport_consumed_length == frame_length);
    assert(decoded_transport_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_transport_frame.cotp.eot == 1);
    assert(decoded_transport_frame.cotp.user_data_length == session_length);
    assert(memcmp(decoded_transport_frame.cotp.user_data, session_buffer, session_length) == 0);

    unitlab_mms_session_spdu_init(&decoded_session_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_session_spdu, decoded_transport_frame.cotp.user_data, decoded_transport_frame.cotp.user_data_length, &session_consumed_length, &diagnostic) == 1);
    assert(session_consumed_length == session_length);
    assert(decoded_session_spdu.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_session_spdu.raw_parameter_length == presentation_length);
    assert(memcmp(decoded_session_spdu.raw_parameter_bytes, presentation_buffer, presentation_length) == 0);

    unitlab_mms_presentation_apdu_init(&decoded_presentation_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_presentation_apdu, decoded_session_spdu.raw_parameter_bytes, decoded_session_spdu.raw_parameter_length, &presentation_consumed_length, &diagnostic) == 1);
    assert(presentation_consumed_length == presentation_length);
    assert(decoded_presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_presentation_apdu.payload_length == acse_length);
    assert(memcmp(decoded_presentation_apdu.payload_bytes, acse_buffer, acse_length) == 0);

    unitlab_mms_acse_apdu_init(&decoded_acse_apdu);
    assert(unitlab_mms_acse_decode(&decoded_acse_apdu, decoded_presentation_apdu.payload_bytes, decoded_presentation_apdu.payload_length, &acse_consumed_length, &diagnostic) == 1);
    assert(acse_consumed_length == acse_length);
    assert(decoded_acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARQ);
    assert(decoded_acse_apdu.apdu_length == sizeof(acse_payload));
    assert(memcmp(decoded_acse_apdu.apdu_bytes, acse_payload, sizeof(acse_payload)) == 0);
}
static void test_association_frame_fully_encoded_layers_roundtrip(void)
{
    uint8_t presentation_buffer[96];
    uint8_t session_buffer[128];
    uint8_t frame_buffer[160];
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsPresentationApdu decoded_presentation_apdu;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsSessionSpdu decoded_session_spdu;
    UnitLabMmsTransportFrame decoded_transport_frame;
    UnitLabMmsAssociationFrame frame;
    UnitLabMmsAssociationFrame decoded_frame;
    const uint8_t payload[] = { 0x30U, 0x02U, 0x01U, 0x01U };
    size_t presentation_length = 0U;
    size_t session_length = 0U;
    size_t frame_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    size_t decoded_consumed_length = 123U;
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

    unitlab_mms_association_frame_init(&frame);
    unitlab_mms_transport_frame_init(&decoded_transport_frame);
    frame.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.transport.cotp.eot = 1;
    frame.transport.cotp.user_data = session_buffer;
    frame.transport.cotp.user_data_length = session_length;
    frame.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    frame.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    frame.presentation.context_identifier = 7U;
    frame.presentation.payload_bytes = payload;
    frame.presentation.payload_length = sizeof(payload);
    assert(unitlab_mms_association_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&decoded_frame);
    assert(unitlab_mms_association_frame_decode(&decoded_frame, frame_buffer, frame_length, &decoded_consumed_length, &diagnostic) == 1);
    assert(decoded_consumed_length == frame_length);
    assert(decoded_frame.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_frame.transport.cotp.eot == 1);
    assert(decoded_frame.session.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(decoded_frame.presentation.context_identifier == 7U);
    assert(decoded_frame.presentation.payload_length == sizeof(payload));
    assert(memcmp(decoded_frame.presentation.payload_bytes, payload, sizeof(payload)) == 0);

    assert(unitlab_mms_transport_frame_decode(&decoded_transport_frame, frame_buffer, frame_length, &transport_consumed_length, &diagnostic) == 1);
    assert(transport_consumed_length == frame_length);
    assert(decoded_transport_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_transport_frame.cotp.user_data_length == session_length);
    assert(memcmp(decoded_transport_frame.cotp.user_data, session_buffer, session_length) == 0);

    unitlab_mms_session_spdu_init(&decoded_session_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_session_spdu, decoded_transport_frame.cotp.user_data, decoded_transport_frame.cotp.user_data_length, &session_consumed_length, &diagnostic) == 1);
    assert(session_consumed_length == session_length);
    assert(decoded_session_spdu.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_session_spdu.raw_parameter_length == presentation_length);
    assert(memcmp(decoded_session_spdu.raw_parameter_bytes, presentation_buffer, presentation_length) == 0);

    unitlab_mms_presentation_apdu_init(&decoded_presentation_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_presentation_apdu, decoded_session_spdu.raw_parameter_bytes, decoded_session_spdu.raw_parameter_length, &presentation_consumed_length, &diagnostic) == 1);
    assert(presentation_consumed_length == presentation_length);
    assert(decoded_presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(decoded_presentation_apdu.context_identifier == 7U);
    assert(decoded_presentation_apdu.payload_length == sizeof(payload));
    assert(memcmp(decoded_presentation_apdu.payload_bytes, payload, sizeof(payload)) == 0);
}
static void test_association_frame_accept_layers_roundtrip(void)
{
    uint8_t acse_buffer[64];
    uint8_t presentation_buffer[96];
    uint8_t session_buffer[128];
    uint8_t frame_buffer[160];
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsAcseApdu decoded_acse_apdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsPresentationApdu decoded_presentation_apdu;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsSessionSpdu decoded_session_spdu;
    UnitLabMmsTransportFrame decoded_transport_frame;
    UnitLabMmsAssociationFrame frame;
    UnitLabMmsAssociationFrame decoded_frame;
    const uint8_t acse_payload[] = { 0x80U, 0x01U, 0x22U };
    size_t acse_length = 0U;
    size_t presentation_length = 0U;
    size_t session_length = 0U;
    size_t frame_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    size_t acse_consumed_length = 0U;
    size_t decoded_consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARE;
    acse_apdu.apdu_bytes = acse_payload;
    acse_apdu.apdu_length = sizeof(acse_payload);
    assert(unitlab_mms_acse_encode(&acse_apdu, acse_buffer, sizeof(acse_buffer), &acse_length, &diagnostic) == 1);

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    presentation_apdu.payload_bytes = acse_buffer;
    presentation_apdu.payload_length = acse_length;
    assert(unitlab_mms_presentation_encode(&presentation_apdu, presentation_buffer, sizeof(presentation_buffer), &presentation_length, &diagnostic) == 1);

    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_ACCEPT;
    session_spdu.spdu_bytes = presentation_buffer;
    session_spdu.spdu_length = presentation_length;
    assert(unitlab_mms_session_spdu_encode(&session_spdu, session_buffer, sizeof(session_buffer), &session_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&frame);
    unitlab_mms_transport_frame_init(&decoded_transport_frame);
    frame.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.transport.cotp.eot = 1;
    frame.transport.cotp.user_data = session_buffer;
    frame.transport.cotp.user_data_length = session_length;
    frame.session.kind = UNITLAB_MMS_SESSION_SPDU_ACCEPT;
    frame.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    frame.presentation.payload_bytes = acse_buffer;
    frame.presentation.payload_length = acse_length;
    assert(unitlab_mms_association_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&decoded_frame);
    assert(unitlab_mms_association_frame_decode(&decoded_frame, frame_buffer, frame_length, &decoded_consumed_length, &diagnostic) == 1);
    assert(decoded_consumed_length == frame_length);
    assert(decoded_frame.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_frame.transport.cotp.eot == 1);
    assert(decoded_frame.session.kind == UNITLAB_MMS_SESSION_SPDU_ACCEPT);
    assert(decoded_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_frame.presentation.payload_length == acse_length);
    assert(memcmp(decoded_frame.presentation.payload_bytes, acse_buffer, acse_length) == 0);

    assert(unitlab_mms_transport_frame_decode(&decoded_transport_frame, frame_buffer, frame_length, &transport_consumed_length, &diagnostic) == 1);
    assert(transport_consumed_length == frame_length);
    assert(decoded_transport_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_transport_frame.cotp.user_data_length == session_length);
    assert(memcmp(decoded_transport_frame.cotp.user_data, session_buffer, session_length) == 0);

    unitlab_mms_session_spdu_init(&decoded_session_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_session_spdu, decoded_transport_frame.cotp.user_data, decoded_transport_frame.cotp.user_data_length, &session_consumed_length, &diagnostic) == 1);
    assert(session_consumed_length == session_length);
    assert(decoded_session_spdu.kind == UNITLAB_MMS_SESSION_SPDU_ACCEPT);
    assert(decoded_session_spdu.raw_parameter_length == presentation_length);
    assert(memcmp(decoded_session_spdu.raw_parameter_bytes, presentation_buffer, presentation_length) == 0);

    unitlab_mms_presentation_apdu_init(&decoded_presentation_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_presentation_apdu, decoded_session_spdu.raw_parameter_bytes, decoded_session_spdu.raw_parameter_length, &presentation_consumed_length, &diagnostic) == 1);
    assert(presentation_consumed_length == presentation_length);
    assert(decoded_presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_presentation_apdu.payload_length == acse_length);
    assert(memcmp(decoded_presentation_apdu.payload_bytes, acse_buffer, acse_length) == 0);

    unitlab_mms_acse_apdu_init(&decoded_acse_apdu);
    assert(unitlab_mms_acse_decode(&decoded_acse_apdu, decoded_presentation_apdu.payload_bytes, decoded_presentation_apdu.payload_length, &acse_consumed_length, &diagnostic) == 1);
    assert(acse_consumed_length == acse_length);
    assert(decoded_acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARE);
    assert(decoded_acse_apdu.apdu_length == sizeof(acse_payload));
    assert(memcmp(decoded_acse_apdu.apdu_bytes, acse_payload, sizeof(acse_payload)) == 0);
}
static void test_association_frame_decode_rejects_non_dt_cotp_frame(void)
{
    uint8_t frame_buffer[64];
    UnitLabMmsTransportFrame frame;
    UnitLabMmsAssociationFrame association_frame;
    size_t frame_length = 0U;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t parameters[] = { 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_CR;
    frame.cotp.destination_reference = 0U;
    frame.cotp.source_reference = 1U;
    frame.cotp.tpdu_class = 0U;
    frame.cotp.user_data = parameters;
    frame.cotp.user_data_length = sizeof(parameters);
    assert(unitlab_mms_transport_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&association_frame);
    association_frame.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    association_frame.transport.cotp.user_data = (const uint8_t*)0x1;
    association_frame.transport.cotp.user_data_length = 99U;
    association_frame.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    association_frame.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    association_frame.presentation.payload_bytes = (const uint8_t*)0x2;
    association_frame.presentation.payload_length = 77U;
    assert(unitlab_mms_association_frame_decode(&association_frame, frame_buffer, frame_length, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(association_frame.transport.tpkt.length == 0U);
    assert(association_frame.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_NONE);
    assert(association_frame.session.kind == UNITLAB_MMS_SESSION_SPDU_NONE);
    assert(association_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_NONE);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}
static void test_association_frame_decode_rejects_malformed_session_payload(void)
{
    uint8_t frame_buffer[64];
    UnitLabMmsTransportFrame frame;
    UnitLabMmsAssociationFrame association_frame;
    size_t frame_length = 0U;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t malformed_session_payload[] = { 0x01U, 0x01U, 0x01U, 0x00U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = malformed_session_payload;
    frame.cotp.user_data_length = sizeof(malformed_session_payload);
    assert(unitlab_mms_transport_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&association_frame);
    association_frame.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    association_frame.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    association_frame.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    assert(unitlab_mms_association_frame_decode(&association_frame, frame_buffer, frame_length, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(association_frame.transport.tpkt.length == 0U);
    assert(association_frame.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_NONE);
    assert(association_frame.session.kind == UNITLAB_MMS_SESSION_SPDU_NONE);
    assert(association_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_NONE);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}
static void test_association_frame_decode_rejects_malformed_presentation_payload(void)
{
    uint8_t session_bytes[32];
    uint8_t frame_buffer[64];
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsTransportFrame frame;
    UnitLabMmsAssociationFrame association_frame;
    size_t session_length = 0U;
    size_t frame_length = 0U;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t malformed_presentation_payload[] = { 0x61U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&session_spdu);
    session_spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    session_spdu.spdu_bytes = malformed_presentation_payload;
    session_spdu.spdu_length = sizeof(malformed_presentation_payload);
    assert(unitlab_mms_session_spdu_encode(&session_spdu, session_bytes, sizeof(session_bytes), &session_length, &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = session_bytes;
    frame.cotp.user_data_length = session_length;
    assert(unitlab_mms_transport_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&association_frame);
    association_frame.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    association_frame.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    association_frame.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    assert(unitlab_mms_association_frame_decode(&association_frame, frame_buffer, frame_length, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(association_frame.transport.tpkt.length == 0U);
    assert(association_frame.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_NONE);
    assert(association_frame.session.kind == UNITLAB_MMS_SESSION_SPDU_NONE);
    assert(association_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_NONE);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL || diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}
static void test_association_frame_decode_resets_output_on_failure(void)
{
    uint8_t frame_buffer[32];
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsTransportFrame frame;
    size_t frame_length = 0U;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[] = { 0xAAU, 0xBBU };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = payload;
    frame.cotp.user_data_length = sizeof(payload);
    assert(unitlab_mms_transport_frame_encode(&frame, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_association_frame_init(&association_frame);
    association_frame.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_CR;
    association_frame.transport.cotp.user_data = (const uint8_t*)0x1;
    association_frame.transport.cotp.user_data_length = 99U;
    association_frame.session.kind = UNITLAB_MMS_SESSION_SPDU_ACCEPT;
    association_frame.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    association_frame.presentation.payload_bytes = (const uint8_t*)0x2;
    association_frame.presentation.payload_length = 77U;
    association_frame.encoded_length = 88U;
    assert(unitlab_mms_association_frame_decode(&association_frame, frame_buffer, frame_length, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(association_frame.transport.tpkt.length == 0U);
    assert(association_frame.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_NONE);
    assert(association_frame.session.kind == UNITLAB_MMS_SESSION_SPDU_NONE);
    assert(association_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_NONE);
    assert(association_frame.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR || diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED || diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL);
}
static void test_association_frame_encode_rejects_missing_presentation_payload(void)
{
    uint8_t frame_buffer[64];
    UnitLabMmsAssociationFrame association_frame;
    size_t encoded_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_association_frame_init(&association_frame);
    association_frame.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    association_frame.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    association_frame.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    association_frame.presentation.payload_bytes = NULL;
    association_frame.presentation.payload_length = 1U;
    assert(unitlab_mms_association_frame_encode(&association_frame, frame_buffer, sizeof(frame_buffer), &encoded_length, &diagnostic) == 0);
    assert(encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT);
}
int main(void)
{
    test_association_frame_decode_roundtrip();
    test_association_frame_fully_encoded_roundtrip();
    test_association_frame_encode_roundtrip();
    test_association_frame_simply_encoded_layers_roundtrip();
    test_association_frame_fully_encoded_layers_roundtrip();
    test_association_frame_accept_layers_roundtrip();
    test_association_frame_decode_rejects_non_dt_cotp_frame();
    test_association_frame_decode_rejects_malformed_session_payload();
    test_association_frame_decode_rejects_malformed_presentation_payload();
    test_association_frame_decode_resets_output_on_failure();
    test_association_frame_encode_rejects_missing_presentation_payload();
    return 0;
}
