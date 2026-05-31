#include "../src/wire/acse/unitlab_mms_acse.h"
#include "../src/wire/session/unitlab_mms_session_spdu.h"
#include "../src/wire/ber/unitlab_mms_ber.h"
#include "../src/wire/presentation/unitlab_mms_presentation.h"
#include "../src/wire/transport/unitlab_mms_transport_frame.h"
#include "../src/wire/transport/unitlab_mms_wire_association_fixture.h"
#include "../src/wire/mms/unitlab_mms_pdu.h"
#include "../src/wire/iso/unitlab_mms_cotp.h"
#include "../src/wire/iso/unitlab_mms_tpkt.h"

#include <assert.h>
#include <string.h>

static void test_tpkt_roundtrip(void)
{
    uint8_t frame[16];
    const uint8_t payload[4] = { 0x11U, 0x22U, 0x33U, 0x44U };
    const uint8_t* decoded_payload = NULL;
    size_t frame_length = 0U;
    size_t payload_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_wrap(payload, sizeof(payload), frame, sizeof(frame), &frame_length, &diagnostic) == 1);
    assert(frame_length == 8U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(unitlab_mms_tpkt_unwrap(frame, frame_length + 4U, &decoded_payload, &payload_length, &consumed_length, &diagnostic) == 1);
    assert(payload_length == sizeof(payload));
    assert(consumed_length == frame_length);
    assert(memcmp(decoded_payload, payload, sizeof(payload)) == 0);
}

static void test_tpkt_unwrap_ignores_trailing_bytes(void)
{
    uint8_t frame[16];
    const uint8_t payload[3] = { 0xAAU, 0xBBU, 0xCCU };
    const uint8_t* decoded_payload = NULL;
    size_t frame_length = 0U;
    size_t payload_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_wrap(payload, sizeof(payload), frame, sizeof(frame), &frame_length, &diagnostic) == 1);
    frame[frame_length + 0U] = 0xDEU;
    frame[frame_length + 1U] = 0xADU;
    frame[frame_length + 2U] = 0xBEU;
    frame[frame_length + 3U] = 0xEFU;
    assert(unitlab_mms_tpkt_unwrap(frame, frame_length + 4U, &decoded_payload, &payload_length, &consumed_length, &diagnostic) == 1);
    assert(payload_length == sizeof(payload));
    assert(consumed_length == frame_length);
    assert(memcmp(decoded_payload, payload, sizeof(payload)) == 0);
}

static void test_tpkt_rejects_invalid_version(void)
{
    const uint8_t frame[4] = { 2U, 0U, 0U, 4U };
    const uint8_t* decoded_payload = NULL;
    size_t payload_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_unwrap(frame, sizeof(frame), &decoded_payload, &payload_length, &consumed_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}

static void test_cotp_cr_roundtrip(void)
{
    uint8_t buffer[32];
    UnitLabMmsCotpTpdu tpdu;
    UnitLabMmsCotpTpdu decoded_tpdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t user_data[3] = { 0xAAU, 0xBBU, 0xCCU };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_cotp_tpdu_init(&tpdu);
    tpdu.kind = UNITLAB_MMS_COTP_TPDU_CR;
    tpdu.destination_reference = 0U;
    tpdu.source_reference = 0x1234U;
    tpdu.tpdu_class = 0x00U;
    tpdu.user_data = user_data;
    tpdu.user_data_length = sizeof(user_data);
    assert(unitlab_mms_cotp_encode(&tpdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == 11U);
    assert(buffer[0] == 10U);
    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tpdu.kind == UNITLAB_MMS_COTP_TPDU_CR);
    assert(decoded_tpdu.source_reference == 0x1234U);
    assert(decoded_tpdu.payload_bytes == &buffer[2]);
    assert(decoded_tpdu.payload_length == encoded_length - 2U);
    assert(decoded_tpdu.user_data_length == sizeof(user_data));
    assert(memcmp(decoded_tpdu.user_data, user_data, sizeof(user_data)) == 0);
}

static void test_cotp_decode_stops_at_indicated_length(void)
{
    uint8_t buffer[16];
    UnitLabMmsCotpTpdu tpdu;
    UnitLabMmsCotpTpdu decoded_tpdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t user_data[2] = { 0x01U, 0x02U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_cotp_tpdu_init(&tpdu);
    tpdu.kind = UNITLAB_MMS_COTP_TPDU_DT;
    tpdu.eot = 1;
    tpdu.user_data = user_data;
    tpdu.user_data_length = sizeof(user_data);
    assert(unitlab_mms_cotp_encode(&tpdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    buffer[encoded_length + 0U] = 0xDEU;
    buffer[encoded_length + 1U] = 0xADU;
    buffer[encoded_length + 2U] = 0xBEU;
    buffer[encoded_length + 3U] = 0xEFU;
    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, buffer, encoded_length + 4U, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tpdu.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_tpdu.eot == 1);
    assert(decoded_tpdu.payload_length == encoded_length - 2U);
    assert(memcmp(decoded_tpdu.payload_bytes, &buffer[2], decoded_tpdu.payload_length) == 0);
}

static void test_cotp_dt_roundtrip(void)
{
    uint8_t buffer[32];
    UnitLabMmsCotpTpdu tpdu;
    UnitLabMmsCotpTpdu decoded_tpdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t user_data[2] = { 0x01U, 0x02U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_cotp_tpdu_init(&tpdu);
    tpdu.kind = UNITLAB_MMS_COTP_TPDU_DT;
    tpdu.eot = 1;
    tpdu.user_data = user_data;
    tpdu.user_data_length = sizeof(user_data);
    assert(unitlab_mms_cotp_encode(&tpdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(buffer[0] == 5U);
    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tpdu.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_tpdu.eot == 1);
    assert(decoded_tpdu.payload_bytes == &buffer[2]);
    assert(decoded_tpdu.payload_length == encoded_length - 2U);
    assert(decoded_tpdu.user_data_length == sizeof(user_data));
    assert(memcmp(decoded_tpdu.user_data, user_data, sizeof(user_data)) == 0);
}

static void test_presentation_raw_roundtrip_preserves_outer_tag(void)
{
    uint8_t buffer[32];
    UnitLabMmsPresentationApdu apdu;
    UnitLabMmsPresentationApdu decoded_apdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[4] = { 0x02U, 0x01U, 0x03U, 0xA0U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_RAW;
    apdu.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    apdu.tag.constructed = 1;
    apdu.tag.tag_number = 7U;
    apdu.payload_bytes = payload;
    apdu.payload_length = sizeof(payload);
    assert(unitlab_mms_presentation_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_RAW);
    assert(decoded_apdu.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_apdu.tag.constructed == 1);
    assert(decoded_apdu.tag.tag_number == 7U);
    assert(decoded_apdu.payload_length == sizeof(payload));
    assert(memcmp(decoded_apdu.payload_bytes, payload, sizeof(payload)) == 0);
}

static void test_presentation_decode_accepts_arbitrary_outer_tag_as_raw(void)
{
    uint8_t buffer[32];
    UnitLabMmsBerElement element;
    UnitLabMmsPresentationApdu decoded_apdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[2] = { 0x30U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_PRIVATE;
    element.tag.constructed = 0;
    element.tag.tag_number = 99U;
    element.value_bytes = payload;
    element.value_length = sizeof(payload);
    assert(unitlab_mms_ber_write(&element, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_RAW);
    assert(decoded_apdu.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_PRIVATE);
    assert(decoded_apdu.tag.constructed == 0);
    assert(decoded_apdu.tag.tag_number == 99U);
    assert(decoded_apdu.payload_length == sizeof(payload));
    assert(memcmp(decoded_apdu.payload_bytes, payload, sizeof(payload)) == 0);
}

static void test_presentation_encode_rejects_non_raw_kind(void)
{
    uint8_t buffer[16];
    UnitLabMmsPresentationApdu apdu;
    size_t encoded_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_UNKNOWN;
    assert(unitlab_mms_presentation_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
}

static void test_presentation_encode_rejects_null_payload_bytes(void)
{
    uint8_t buffer[16];
    UnitLabMmsPresentationApdu apdu;
    size_t encoded_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_RAW;
    apdu.payload_bytes = NULL;
    apdu.payload_length = 1U;
    apdu.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
    apdu.tag.constructed = 1;
    apdu.tag.tag_number = 2U;
    assert(unitlab_mms_presentation_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT);
}

static void test_presentation_rejects_truncated_ber(void)
{
    const uint8_t buffer[1] = { 0xA1U };
    UnitLabMmsPresentationApdu decoded_apdu;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL || diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}

static void test_wire_association_fixture_decode_roundtrip(void)
{
    uint8_t acse_buffer[64];
    uint8_t presentation_buffer[96];
    uint8_t frame_buffer[128];
    UnitLabMmsPdu mms_pdu;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsWireAssociationFixture fixture;
    UnitLabMmsWireAssociationFixture decoded_fixture;
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
    presentation_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_RAW;
    presentation_apdu.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    presentation_apdu.tag.constructed = 1;
    presentation_apdu.tag.tag_number = 7U;
    presentation_apdu.payload_bytes = acse_buffer;
    presentation_apdu.payload_length = acse_length;
    assert(unitlab_mms_presentation_encode(&presentation_apdu, presentation_buffer, sizeof(presentation_buffer), &presentation_length, &diagnostic) == 1);

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_RAW;
    fixture.presentation.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    fixture.presentation.tag.constructed = 1;
    fixture.presentation.tag.tag_number = 7U;
    fixture.presentation.payload_bytes = acse_buffer;
    fixture.presentation.payload_length = acse_length;
    assert(unitlab_mms_wire_association_fixture_encode(&fixture, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_wire_association_fixture_init(&decoded_fixture);
    assert(unitlab_mms_wire_association_fixture_decode(&decoded_fixture, frame_buffer, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_fixture.transport.tpkt.version == 3U);
    assert(decoded_fixture.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_RAW);
    assert(decoded_fixture.presentation.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_fixture.presentation.tag.tag_number == 7U);
    assert(decoded_fixture.presentation.payload_length == acse_length);
    assert(memcmp(decoded_fixture.presentation.payload_bytes, acse_buffer, acse_length) == 0);
}

static void test_wire_association_fixture_encode_roundtrip(void)
{
    uint8_t acse_buffer[64];
    uint8_t frame_buffer[128];
    UnitLabMmsPdu mms_pdu;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsWireAssociationFixture fixture;
    UnitLabMmsWireAssociationFixture decoded_fixture;
    size_t mms_length = 0U;
    size_t acse_length = 0U;
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

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_RAW;
    fixture.presentation.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    fixture.presentation.tag.constructed = 1;
    fixture.presentation.tag.tag_number = 7U;
    fixture.presentation.payload_bytes = acse_buffer;
    fixture.presentation.payload_length = acse_length;
    assert(unitlab_mms_wire_association_fixture_encode(&fixture, frame_buffer, sizeof(frame_buffer), &frame_length, &diagnostic) == 1);

    unitlab_mms_wire_association_fixture_init(&decoded_fixture);
    assert(unitlab_mms_wire_association_fixture_decode(&decoded_fixture, frame_buffer, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_fixture.transport.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_RAW);
    assert(decoded_fixture.presentation.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_fixture.presentation.tag.tag_number == 7U);
    assert(decoded_fixture.presentation.payload_length == acse_length);
    assert(memcmp(decoded_fixture.presentation.payload_bytes, acse_buffer, acse_length) == 0);
}

static void test_transport_frame_roundtrip(void)
{
    uint8_t buffer[64];
    UnitLabMmsTransportFrame frame;
    UnitLabMmsTransportFrame decoded_frame;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t user_data[3] = { 0xAAU, 0xBBU, 0xCCU };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = user_data;
    frame.cotp.user_data_length = sizeof(user_data);
    assert(unitlab_mms_transport_frame_encode(&frame, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_frame.tpkt.version == 3U);
    assert(decoded_frame.tpkt.reserved == 0U);
    assert(decoded_frame.tpkt.length == encoded_length);
    assert(decoded_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_frame.cotp.eot == 1);
    assert(decoded_frame.cotp.user_data_length == sizeof(user_data));
    assert(memcmp(decoded_frame.cotp.user_data, user_data, sizeof(user_data)) == 0);
}

static void test_acse_top_level_roundtrips(void)
{
    struct {
        UnitLabMmsAcseApduKind kind;
        uint8_t tag;
    } cases[] = {
        { UNITLAB_MMS_ACSE_APDU_AARQ, 0x60U },
        { UNITLAB_MMS_ACSE_APDU_AARE, 0x61U },
        { UNITLAB_MMS_ACSE_APDU_RLRQ, 0x62U },
        { UNITLAB_MMS_ACSE_APDU_RLRE, 0x63U },
        { UNITLAB_MMS_ACSE_APDU_ABRT, 0x64U },
    };
    uint8_t buffer[32];
    UnitLabMmsAcseApdu apdu;
    UnitLabMmsAcseApdu decoded_apdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[4] = { 0x30U, 0x02U, 0x01U, 0x01U };

    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        unitlab_mms_diagnostic_clear(&diagnostic);
        unitlab_mms_acse_apdu_init(&apdu);
        apdu.kind = cases[i].kind;
        apdu.apdu_bytes = payload;
        apdu.apdu_length = sizeof(payload);
        assert(unitlab_mms_acse_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
        assert(buffer[0] == cases[i].tag);
        unitlab_mms_acse_apdu_init(&decoded_apdu);
        assert(unitlab_mms_acse_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
        assert(consumed_length == encoded_length);
        assert(decoded_apdu.kind == cases[i].kind);
        assert(decoded_apdu.apdu_length == sizeof(payload));
        assert(memcmp(decoded_apdu.apdu_bytes, payload, sizeof(payload)) == 0);
    }
}

static void test_session_spdu_roundtrip_long_length_indicator(void)
{
    uint8_t buffer[400];
    UnitLabMmsSessionSpdu spdu;
    UnitLabMmsSessionSpdu decoded_spdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    uint8_t payload[1U + 3U + 260U];

    payload[0] = 1U;
    payload[1] = 0xFFU;
    payload[2] = 0x01U;
    payload[3] = 0x04U;
    for (size_t i = 0U; i < 260U; i++) {
        payload[4U + i] = (uint8_t)(i & 0xFFU);
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    spdu.spdu_bytes = payload;
    spdu.spdu_length = sizeof(payload);
    assert(unitlab_mms_session_spdu_encode(&spdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == sizeof(payload));
    unitlab_mms_session_spdu_init(&decoded_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_spdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_spdu.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_spdu.spdu_length == sizeof(payload));
    assert(decoded_spdu.raw_parameter_length == 260U);
    assert(memcmp(decoded_spdu.raw_parameter_bytes, &payload[4], 260U) == 0);
}

static void test_session_spdu_decode_stops_at_indicated_length(void)
{
    uint8_t buffer[16];
    UnitLabMmsSessionSpdu spdu;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t raw_spdu[5] = { 1U, 3U, 0xAAU, 0xBBU, 0xCCU };

    memcpy(buffer, raw_spdu, sizeof(raw_spdu));
    buffer[5] = 0xDEU;
    buffer[6] = 0xADU;
    buffer[7] = 0xBEU;
    buffer[8] = 0xEFU;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    assert(unitlab_mms_session_spdu_decode(&spdu, buffer, sizeof(raw_spdu) + 4U, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == sizeof(raw_spdu));
    assert(spdu.spdu_length == sizeof(raw_spdu));
    assert(spdu.raw_parameter_length == 3U);
    assert(memcmp(spdu.raw_parameter_bytes, &raw_spdu[2], 3U) == 0);
}

static void test_session_spdu_roundtrips(void)
{
    struct {
        UnitLabMmsSessionSpduKind kind;
        uint8_t code;
        const uint8_t payload[5];
        size_t payload_length;
    } cases[] = {
        { UNITLAB_MMS_SESSION_SPDU_CONNECT, 13U, { 13U, 3U, 0x01U, 0x02U, 0x03U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_CONNECT_DATA_OVERFLOW, 15U, { 15U, 3U, 0xAAU, 0xBBU, 0xCCU }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_OVERFLOW_ACCEPT, 16U, { 16U, 3U, 0x11U, 0x22U, 0x33U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_ACCEPT, 14U, { 14U, 3U, 0x44U, 0x55U, 0x66U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_REFUSE, 12U, { 12U, 3U, 0x77U, 0x88U, 0x99U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_FINISH, 9U, { 9U, 3U, 0x10U, 0x20U, 0x30U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_DISCONNECT, 10U, { 10U, 3U, 0x40U, 0x50U, 0x60U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_ABORT, 25U, { 25U, 3U, 0x70U, 0x80U, 0x90U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_ABORT_ACCEPT, 26U, { 26U, 0U, 0x00U, 0x00U, 0x00U }, 2U },
        { UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER, 1U, { 1U, 3U, 0x01U, 0x00U, 0xFFU }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_EXPEDITED_DATA, 5U, { 5U, 3U, 0x02U, 0x03U, 0x04U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_TYPED_DATA, 33U, { 33U, 3U, 0x05U, 0x06U, 0x07U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA, 61U, { 61U, 3U, 0x08U, 0x09U, 0x0AU }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA_ACK, 62U, { 62U, 3U, 0x0BU, 0x0CU, 0x0DU }, 5U },
    };
    uint8_t buffer[8];
    UnitLabMmsSessionSpdu spdu;
    UnitLabMmsSessionSpdu decoded_spdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        unitlab_mms_diagnostic_clear(&diagnostic);
        unitlab_mms_session_spdu_init(&spdu);
        spdu.kind = cases[i].kind;
        spdu.spdu_bytes = cases[i].payload;
        spdu.spdu_length = cases[i].payload_length;
        assert(unitlab_mms_session_spdu_encode(&spdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
        assert(encoded_length == cases[i].payload_length);
        assert(buffer[0] == cases[i].code);
        unitlab_mms_session_spdu_init(&decoded_spdu);
        assert(unitlab_mms_session_spdu_decode(&decoded_spdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
        assert(consumed_length == encoded_length);
        assert(decoded_spdu.kind == cases[i].kind);
        assert(decoded_spdu.spdu_length == cases[i].payload_length);
        assert(decoded_spdu.raw_parameter_length == cases[i].payload_length - 2U);
        assert(decoded_spdu.raw_parameter_bytes == &decoded_spdu.spdu_bytes[2]);
        assert(memcmp(decoded_spdu.spdu_bytes, cases[i].payload, cases[i].payload_length) == 0);
    }
}

static void test_session_spdu_rejects_mismatched_declared_kind_and_code(void)
{
    uint8_t buffer[8];
    UnitLabMmsSessionSpdu spdu;
    size_t encoded_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[4] = { 13U, 0x01U, 0x02U, 0x03U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    spdu.kind = UNITLAB_MMS_SESSION_SPDU_ACCEPT;
    spdu.spdu_bytes = payload;
    spdu.spdu_length = sizeof(payload);
    assert(unitlab_mms_session_spdu_encode(&spdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}

static void test_acse_raw_field_view(void)
{
    uint8_t buffer[32];
    UnitLabMmsAcseApdu apdu;
    UnitLabMmsAcseApdu decoded_apdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[8] = { 0x80U, 0x01U, 0x00U, 0x81U, 0x01U, 0x2AU, 0x82U, 0x00U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_acse_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    apdu.apdu_bytes = payload;
    apdu.apdu_length = sizeof(payload);
    assert(unitlab_mms_acse_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    unitlab_mms_acse_apdu_init(&decoded_apdu);
    assert(unitlab_mms_acse_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARQ);
    assert(decoded_apdu.field_count == 3U);
    assert(decoded_apdu.fields[0].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_apdu.fields[0].tag.tag_number == 0U);
    assert(decoded_apdu.fields[0].value_length == 1U);
    assert(decoded_apdu.fields[0].value_bytes[0] == 0x00U);
    assert(decoded_apdu.fields[1].tag.tag_number == 1U);
    assert(decoded_apdu.fields[1].value_length == 1U);
    assert(decoded_apdu.fields[1].value_bytes[0] == 0x2AU);
    assert(decoded_apdu.fields[2].tag.tag_number == 2U);
    assert(decoded_apdu.fields[2].value_length == 0U);
}

static void test_mms_pdu_confirmed_request_roundtrip(void)
{
    uint8_t buffer[32];
    UnitLabMmsPdu pdu;
    UnitLabMmsPdu decoded_pdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[6] = { 0x02U, 0x01U, 0x05U, 0xA4U, 0x01U, 0xAAU };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    pdu.pdu_bytes = payload;
    pdu.pdu_length = sizeof(payload);
    assert(unitlab_mms_pdu_encode(&pdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(buffer[0] == 0x60U);
    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
    assert(decoded_pdu.has_invoke_id == 1);
    assert(decoded_pdu.invoke_id == 5U);
    assert(decoded_pdu.has_service == 1);
    assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
    assert(decoded_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_pdu.service_tag.tag_number == 4U);
    assert(decoded_pdu.service_length == 1U);
    assert(decoded_pdu.service_bytes[0] == 0xAAU);
    assert(decoded_pdu.pdu_length == sizeof(payload));
    assert(memcmp(decoded_pdu.pdu_bytes, payload, sizeof(payload)) == 0);
}

static void test_mms_pdu_confirmed_response_roundtrip(void)
{
    uint8_t buffer[32];
    UnitLabMmsPdu pdu;
    UnitLabMmsPdu decoded_pdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[6] = { 0x02U, 0x01U, 0x06U, 0xA5U, 0x01U, 0xBBU };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    pdu.pdu_bytes = payload;
    pdu.pdu_length = sizeof(payload);
    assert(unitlab_mms_pdu_encode(&pdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(buffer[0] == 0x61U);
    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
    assert(decoded_pdu.has_invoke_id == 1);
    assert(decoded_pdu.invoke_id == 6U);
    assert(decoded_pdu.has_service == 1);
    assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_WRITE);
    assert(decoded_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_pdu.service_tag.tag_number == 5U);
    assert(decoded_pdu.service_length == 1U);
    assert(decoded_pdu.service_bytes[0] == 0xBBU);
    assert(decoded_pdu.pdu_length == sizeof(payload));
    assert(memcmp(decoded_pdu.pdu_bytes, payload, sizeof(payload)) == 0);
}

static void test_mms_pdu_unconfirmed_roundtrip(void)
{
    uint8_t buffer[32];
    UnitLabMmsPdu pdu;
    UnitLabMmsPdu decoded_pdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[3] = { 0xA0U, 0x01U, 0xAAU };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    pdu.pdu_bytes = payload;
    pdu.pdu_length = sizeof(payload);
    assert(unitlab_mms_pdu_encode(&pdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(buffer[0] == 0x63U);
    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_UNCONFIRMED);
    assert(decoded_pdu.has_invoke_id == 0);
    assert(decoded_pdu.has_service == 1);
    assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_INFORMATION_REPORT);
    assert(decoded_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_pdu.service_tag.tag_number == 0U);
    assert(decoded_pdu.service_length == 1U);
    assert(decoded_pdu.service_bytes[0] == 0xAAU);
    assert(decoded_pdu.pdu_length == sizeof(payload));
    assert(memcmp(decoded_pdu.pdu_bytes, payload, sizeof(payload)) == 0);
}

static void test_mms_pdu_confirmed_request_roundtrip_with_wide_invoke_id(void)
{
    uint8_t buffer[32];
    UnitLabMmsPdu pdu;
    UnitLabMmsPdu decoded_pdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[10] = { 0x02U, 0x05U, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xA5U, 0x01U, 0xAAU };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    pdu.pdu_bytes = payload;
    pdu.pdu_length = sizeof(payload);
    assert(unitlab_mms_pdu_encode(&pdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(buffer[0] == 0x60U);
    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
    assert(decoded_pdu.has_invoke_id == 1);
    assert(decoded_pdu.invoke_id == 0xFFFFFFFFU);
    assert(decoded_pdu.has_service == 1);
    assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_WRITE);
    assert(decoded_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_pdu.service_tag.tag_number == 5U);
    assert(decoded_pdu.service_length == 1U);
    assert(decoded_pdu.service_bytes[0] == 0xAAU);
    assert(decoded_pdu.pdu_length == sizeof(payload));
    assert(memcmp(decoded_pdu.pdu_bytes, payload, sizeof(payload)) == 0);
}

static void test_mms_pdu_rejects_non_minimal_invoke_id(void)
{
    const uint8_t buffer[6] = { 0x60U, 0x04U, 0x02U, 0x02U, 0x00U, 0x01U };
    UnitLabMmsPdu decoded_pdu;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}

static void test_ber_length_roundtrip(void)
{
    uint8_t buffer[16];
    size_t encoded_length = 0U;
    size_t decoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_ber_length_encode(0U, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == 1U);
    assert(unitlab_mms_ber_length_decode(&decoded_length, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(decoded_length == 0U);
    assert(consumed_length == 1U);

    assert(unitlab_mms_ber_length_encode(127U, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == 1U);
    assert(unitlab_mms_ber_length_decode(&decoded_length, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(decoded_length == 127U);
    assert(consumed_length == 1U);

    assert(unitlab_mms_ber_length_encode(128U, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == 2U);
    assert(unitlab_mms_ber_length_decode(&decoded_length, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(decoded_length == 128U);
    assert(consumed_length == 2U);

    {
        const uint8_t long_form_small_value[2] = { 0x81U, 0x01U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, long_form_small_value, sizeof(long_form_small_value), &consumed_length, &diagnostic) == 1);
        assert(decoded_length == 1U);
        assert(consumed_length == 2U);
    }

    {
        const uint8_t indefinite_length[1] = { 0x80U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, indefinite_length, sizeof(indefinite_length), &consumed_length, &diagnostic) == 0);
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    }

    {
        const uint8_t truncated_length[2] = { 0x82U, 0x01U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, truncated_length, sizeof(truncated_length), &consumed_length, &diagnostic) == 0);
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL);
    }

    {
        const uint8_t leading_zero_length[3] = { 0x82U, 0x00U, 0x80U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, leading_zero_length, sizeof(leading_zero_length), &consumed_length, &diagnostic) == 0);
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    }

    {
        const uint8_t unsupported_octet_count[10] = { 0x89U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, unsupported_octet_count, sizeof(unsupported_octet_count), &consumed_length, &diagnostic) == 0);
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    }
}

static void test_ber_tag_roundtrip(void)
{
    uint8_t buffer[16];
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsBerTag tag;
    UnitLabMmsBerTag decoded_tag;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_tag_init(&tag);
    tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    tag.constructed = 1;
    tag.tag_number = 5U;
    assert(unitlab_mms_ber_tag_encode(&tag, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == 1U);
    unitlab_mms_ber_tag_init(&decoded_tag);
    assert(unitlab_mms_ber_tag_decode(&decoded_tag, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == 1U);
    assert(decoded_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_tag.constructed == 1);
    assert(decoded_tag.tag_number == 5U);
}

static void test_ber_tag_long_form_valid(void)
{
    const uint8_t tag_bytes[2] = { 0x1FU, 0x1FU };
    UnitLabMmsBerTag decoded_tag;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_tag_init(&decoded_tag);
    assert(unitlab_mms_ber_tag_decode(&decoded_tag, tag_bytes, sizeof(tag_bytes), &consumed_length, &diagnostic) == 1);
    assert(consumed_length == 2U);
    assert(decoded_tag.tag_number == 31U);
}

static void test_ber_tag_rejects_non_minimal_long_form(void)
{
    const uint8_t tag_bytes[2] = { 0x1FU, 0x1EU };
    UnitLabMmsBerTag decoded_tag;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_tag_init(&decoded_tag);
    assert(unitlab_mms_ber_tag_decode(&decoded_tag, tag_bytes, sizeof(tag_bytes), &consumed_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}

static void test_ber_tag_rejects_truncated_long_form(void)
{
    const uint8_t tag_bytes[1] = { 0x1FU };
    UnitLabMmsBerTag decoded_tag;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_tag_init(&decoded_tag);
    assert(unitlab_mms_ber_tag_decode(&decoded_tag, tag_bytes, sizeof(tag_bytes), &consumed_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL);
}

static void test_ber_tag_rejects_overflow_long_form(void)
{
    const uint8_t tag_bytes[7] = { 0x1FU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x7FU };
    UnitLabMmsBerTag decoded_tag;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_tag_init(&decoded_tag);
    assert(unitlab_mms_ber_tag_decode(&decoded_tag, tag_bytes, sizeof(tag_bytes), &consumed_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}

static void test_ber_element_roundtrip(void)
{
    uint8_t buffer[16];
    UnitLabMmsBerElement element;
    UnitLabMmsBerElement decoded_element;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t value[3] = { 0x01U, 0x02U, 0x03U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
    element.tag.constructed = 0;
    element.tag.tag_number = 2U;
    element.value_bytes = value;
    element.value_length = sizeof(value);
    assert(unitlab_mms_ber_write(&element, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == 5U);
    unitlab_mms_ber_element_init(&decoded_element);
    assert(unitlab_mms_ber_read(&decoded_element, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION);
    assert(decoded_element.tag.tag_number == 2U);
    assert(decoded_element.value_length == sizeof(value));
    assert(memcmp(decoded_element.value_bytes, value, sizeof(value)) == 0);
}

static void test_ber_element_rejects_missing_value_bytes(void)
{
    uint8_t buffer[16];
    UnitLabMmsBerElement element;
    size_t encoded_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
    element.tag.constructed = 0;
    element.tag.tag_number = 2U;
    element.value_bytes = NULL;
    element.value_length = 1U;
    assert(unitlab_mms_ber_write(&element, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT);
}

int main(void)
{
    test_tpkt_roundtrip();
    test_tpkt_unwrap_ignores_trailing_bytes();
    test_tpkt_rejects_invalid_version();
    test_cotp_cr_roundtrip();
    test_cotp_dt_roundtrip();
    test_cotp_decode_stops_at_indicated_length();
    test_transport_frame_roundtrip();
    test_wire_association_fixture_decode_roundtrip();
    test_wire_association_fixture_encode_roundtrip();
    test_acse_top_level_roundtrips();
    test_acse_raw_field_view();
    test_session_spdu_roundtrip_long_length_indicator();
    test_session_spdu_decode_stops_at_indicated_length();
    test_session_spdu_roundtrips();
    test_session_spdu_rejects_mismatched_declared_kind_and_code();
    test_session_spdu_rejects_mismatched_declared_kind_and_code();
    test_presentation_raw_roundtrip_preserves_outer_tag();
    test_presentation_decode_accepts_arbitrary_outer_tag_as_raw();
    test_presentation_encode_rejects_non_raw_kind();
    test_presentation_encode_rejects_null_payload_bytes();
    test_presentation_rejects_truncated_ber();
    test_mms_pdu_confirmed_request_roundtrip();
    test_mms_pdu_confirmed_response_roundtrip();
    test_mms_pdu_unconfirmed_roundtrip();
    test_mms_pdu_confirmed_request_roundtrip_with_wide_invoke_id();
    test_mms_pdu_rejects_non_minimal_invoke_id();
    test_ber_length_roundtrip();
    test_ber_tag_roundtrip();
    test_ber_tag_long_form_valid();
    test_ber_tag_rejects_non_minimal_long_form();
    test_ber_tag_rejects_truncated_long_form();
    test_ber_tag_rejects_overflow_long_form();
    test_ber_element_roundtrip();
    test_ber_element_rejects_missing_value_bytes();
    return 0;
}
