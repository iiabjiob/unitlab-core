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

static void test_tpkt_write_header(void)
{
    uint8_t frame[4];
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_write_header(frame, sizeof(frame), 0x0010U, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(memcmp(frame, (const uint8_t[]){ 0x03U, 0x00U, 0x00U, 0x10U }, sizeof(frame)) == 0);
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

static void test_tpkt_minimal_roundtrip(void)
{
    uint8_t frame[4];
    const uint8_t* decoded_payload = NULL;
    size_t frame_length = 0U;
    size_t payload_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_wrap(NULL, 0U, frame, sizeof(frame), &frame_length, &diagnostic) == 1);
    assert(frame_length == 4U);
    assert(memcmp(frame, (const uint8_t[]){ 0x03U, 0x00U, 0x00U, 0x04U }, sizeof(frame)) == 0);
    assert(unitlab_mms_tpkt_unwrap(frame, frame_length, &decoded_payload, &payload_length, &consumed_length, &diagnostic) == 1);
    assert(payload_length == 0U);
    assert(consumed_length == frame_length);
    assert(decoded_payload == &frame[4]);
}

static void test_tpkt_rejects_length_less_than_header(void)
{
    const uint8_t frame[4] = { 0x03U, 0x00U, 0x00U, 0x03U };
    const uint8_t* decoded_payload = NULL;
    size_t payload_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_unwrap(frame, sizeof(frame), &decoded_payload, &payload_length, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
}

static void test_tpkt_rejects_declared_length_larger_than_buffer(void)
{
    const uint8_t frame[4] = { 0x03U, 0x00U, 0x00U, 0x06U };
    const uint8_t* decoded_payload = NULL;
    size_t payload_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_unwrap(frame, sizeof(frame), &decoded_payload, &payload_length, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL);
    assert(diagnostic.message[0] != '\0');
}

static void test_cotp_cr_golden(void)
{
    uint8_t buffer[32];
    UnitLabMmsCotpTpdu tpdu;
    UnitLabMmsCotpTpdu decoded_tpdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t parameters[] = { 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };
    const uint8_t expected[] = { 0x11U, 0xE0U, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U, 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_cotp_tpdu_init(&tpdu);
    tpdu.kind = UNITLAB_MMS_COTP_TPDU_CR;
    tpdu.destination_reference = 0U;
    tpdu.source_reference = 1U;
    tpdu.tpdu_class = 0U;
    tpdu.user_data = parameters;
    tpdu.user_data_length = sizeof(parameters);
    assert(unitlab_mms_cotp_encode(&tpdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == sizeof(expected));
    assert(memcmp(buffer, expected, sizeof(expected)) == 0);
    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tpdu.kind == UNITLAB_MMS_COTP_TPDU_CR);
    assert(decoded_tpdu.destination_reference == 0U);
    assert(decoded_tpdu.source_reference == 1U);
    assert(decoded_tpdu.tpdu_class == 0U);
    assert(decoded_tpdu.user_data_length == sizeof(parameters));
    assert(memcmp(decoded_tpdu.user_data, parameters, sizeof(parameters)) == 0);
}

static void test_cotp_cc_golden(void)
{
    uint8_t buffer[32];
    UnitLabMmsCotpTpdu tpdu;
    UnitLabMmsCotpTpdu decoded_tpdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t parameters[] = { 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };
    const uint8_t expected[] = { 0x11U, 0xD0U, 0x00U, 0x01U, 0x00U, 0x01U, 0x00U, 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_cotp_tpdu_init(&tpdu);
    tpdu.kind = UNITLAB_MMS_COTP_TPDU_CC;
    tpdu.destination_reference = 1U;
    tpdu.source_reference = 1U;
    tpdu.tpdu_class = 0U;
    tpdu.user_data = parameters;
    tpdu.user_data_length = sizeof(parameters);
    assert(unitlab_mms_cotp_encode(&tpdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == sizeof(expected));
    assert(memcmp(buffer, expected, sizeof(expected)) == 0);
    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tpdu.kind == UNITLAB_MMS_COTP_TPDU_CC);
    assert(decoded_tpdu.destination_reference == 1U);
    assert(decoded_tpdu.source_reference == 1U);
    assert(decoded_tpdu.tpdu_class == 0U);
    assert(decoded_tpdu.user_data_length == sizeof(parameters));
    assert(memcmp(decoded_tpdu.user_data, parameters, sizeof(parameters)) == 0);
}

static void test_cotp_rejects_malformed_inputs(void)
{
    const uint8_t empty[1] = { 0U };
    const uint8_t truncated[] = { 0x06U };
    const uint8_t invalid_length_indicator[] = { 0x03U, 0xF0U, 0x80U, 0xAAU };
    const uint8_t unsupported[] = { 0x02U, 0x99U };
    UnitLabMmsCotpTpdu decoded_tpdu;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, empty, 0U, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL);

    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, truncated, sizeof(truncated), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL);

    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, invalid_length_indicator, sizeof(invalid_length_indicator), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');

    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, unsupported, sizeof(unsupported), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
    assert(diagnostic.message[0] != '\0');
}

static void test_cotp_encode_rejects_missing_user_data(void)
{
    uint8_t buffer[8];
    UnitLabMmsCotpTpdu tpdu;
    size_t encoded_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_cotp_tpdu_init(&tpdu);
    tpdu.kind = UNITLAB_MMS_COTP_TPDU_DT;
    tpdu.eot = 1;
    tpdu.user_data = NULL;
    tpdu.user_data_length = 1U;
    assert(unitlab_mms_cotp_encode(&tpdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 0);
    assert(encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(diagnostic.message[0] != '\0');
}

static void test_transport_frame_roundtrip_cr_cc_dt(void)
{
    uint8_t buffer[128];
    UnitLabMmsTransportFrame frame;
    UnitLabMmsTransportFrame decoded_frame;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t parameters[] = { 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U };
    const uint8_t payload[] = { 0x01U, 0x02U, 0x03U };
    struct {
        UnitLabMmsCotpTpduKind kind;
        uint16_t destination_reference;
        uint16_t source_reference;
        uint8_t tpdu_class;
        int eot;
        const uint8_t* user_data;
        size_t user_data_length;
    } cases[] = {
        { UNITLAB_MMS_COTP_TPDU_CR, 0U, 1U, 0U, 0, parameters, sizeof(parameters) },
        { UNITLAB_MMS_COTP_TPDU_CC, 1U, 1U, 0U, 0, parameters, sizeof(parameters) },
        { UNITLAB_MMS_COTP_TPDU_DT, 0U, 0U, 0U, 1, payload, sizeof(payload) },
    };

    unitlab_mms_diagnostic_clear(&diagnostic);
    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        unitlab_mms_transport_frame_init(&frame);
        frame.cotp.kind = cases[i].kind;
        frame.cotp.destination_reference = cases[i].destination_reference;
        frame.cotp.source_reference = cases[i].source_reference;
        frame.cotp.tpdu_class = cases[i].tpdu_class;
        frame.cotp.eot = cases[i].eot;
        frame.cotp.user_data = cases[i].user_data;
        frame.cotp.user_data_length = cases[i].user_data_length;
        assert(unitlab_mms_transport_frame_encode(&frame, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
        unitlab_mms_transport_frame_init(&decoded_frame);
        assert(unitlab_mms_transport_frame_decode(&decoded_frame, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
        assert(consumed_length == encoded_length);
        assert(decoded_frame.tpkt.version == 3U);
        assert(decoded_frame.tpkt.reserved == 0U);
        assert(decoded_frame.tpkt.length == encoded_length);
        assert(decoded_frame.cotp.kind == cases[i].kind);
        assert(decoded_frame.cotp.user_data_length == cases[i].user_data_length);
        assert(memcmp(decoded_frame.cotp.user_data, cases[i].user_data, cases[i].user_data_length) == 0);
        if (cases[i].kind == UNITLAB_MMS_COTP_TPDU_DT) {
            assert(decoded_frame.cotp.eot == 1);
            assert(decoded_frame.cotp.payload_length == encoded_length - 5U);
        } else {
            assert(decoded_frame.cotp.destination_reference == cases[i].destination_reference);
            assert(decoded_frame.cotp.source_reference == cases[i].source_reference);
            assert(decoded_frame.cotp.tpdu_class == cases[i].tpdu_class);
        }
    }
}

static void test_transport_frame_rejects_trailing_bytes(void)
{
    uint8_t buffer[32];
    UnitLabMmsTransportFrame frame;
    UnitLabMmsTransportFrame decoded_frame;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[2] = { 0xAAU, 0xBBU };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_transport_frame_init(&frame);
    frame.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    frame.cotp.eot = 1;
    frame.cotp.user_data = payload;
    frame.cotp.user_data_length = sizeof(payload);
    assert(unitlab_mms_transport_frame_encode(&frame, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    buffer[encoded_length + 0U] = 0xDEU;
    buffer[encoded_length + 1U] = 0xADU;
    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, buffer, encoded_length + 2U, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
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
    assert(encoded_length == 10U);
    assert(buffer[0] == 9U);
    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tpdu.kind == UNITLAB_MMS_COTP_TPDU_CR);
    assert(decoded_tpdu.source_reference == 0x1234U);
    assert(decoded_tpdu.payload_bytes == &buffer[1]);
    assert(decoded_tpdu.payload_length == encoded_length - 1U);
    assert(decoded_tpdu.user_data_length == sizeof(user_data));
    assert(memcmp(decoded_tpdu.user_data, user_data, sizeof(user_data)) == 0);
}

static void test_cotp_connect_request_frame_smoke(void)
{
    uint8_t frame[32];
    UnitLabMmsTransportFrame decoded_frame;
    size_t frame_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_build_cotp_connect_request_frame(frame, sizeof(frame), &frame_length, &diagnostic) == 1);
    assert(frame_length == 22U);
    assert(memcmp(frame, (const uint8_t[]){ 0x03U, 0x00U, 0x00U, 0x16U, 0x11U, 0xE0U, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U, 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U }, frame_length) == 0);
    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, frame, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_CR);
    assert(decoded_frame.cotp.destination_reference == 0U);
    assert(decoded_frame.cotp.source_reference == 1U);
    assert(decoded_frame.cotp.tpdu_class == 0U);
    assert(decoded_frame.cotp.user_data_length == 11U);
    assert(memcmp(decoded_frame.cotp.user_data, (const uint8_t[]){ 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U }, 11U) == 0);
}

static void test_cotp_connect_response_frame_smoke(void)
{
    uint8_t frame[32];
    UnitLabMmsTransportFrame decoded_frame;
    size_t frame_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_build_cotp_connect_response_frame(frame, sizeof(frame), &frame_length, &diagnostic) == 1);
    assert(frame_length == 22U);
    assert(memcmp(frame, (const uint8_t[]){ 0x03U, 0x00U, 0x00U, 0x16U, 0x11U, 0xD0U, 0x00U, 0x01U, 0x00U, 0x01U, 0x00U, 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U }, frame_length) == 0);
    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, frame, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_CC);
    assert(decoded_frame.cotp.destination_reference == 1U);
    assert(decoded_frame.cotp.source_reference == 1U);
    assert(decoded_frame.cotp.tpdu_class == 0U);
    assert(decoded_frame.cotp.user_data_length == 11U);
    assert(memcmp(decoded_frame.cotp.user_data, (const uint8_t[]){ 0xC0U, 0x01U, 0x0DU, 0xC2U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x02U, 0x00U, 0x01U }, 11U) == 0);
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
    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tpdu.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_tpdu.eot == 1);
    assert(decoded_tpdu.payload_length == encoded_length - 1U);
    assert(memcmp(decoded_tpdu.payload_bytes, &buffer[1], decoded_tpdu.payload_length) == 0);
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
    assert(buffer[0] == 2U);
    assert(buffer[1] == 0xF0U);
    assert(buffer[2] == 0x80U);
    assert(buffer[3] == user_data[0]);
    assert(buffer[4] == user_data[1]);
    unitlab_mms_cotp_tpdu_init(&decoded_tpdu);
    assert(unitlab_mms_cotp_decode(&decoded_tpdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tpdu.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_tpdu.eot == 1);
    assert(decoded_tpdu.payload_bytes == &buffer[1]);
    assert(decoded_tpdu.payload_length == encoded_length - 1U);
    assert(decoded_tpdu.user_data_length == sizeof(user_data));
    assert(memcmp(decoded_tpdu.user_data, user_data, sizeof(user_data)) == 0);
}

static void test_presentation_simply_encoded_roundtrip(void)
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
    apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    apdu.payload_bytes = payload;
    apdu.payload_length = sizeof(payload);
    assert(unitlab_mms_presentation_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_apdu.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION);
    assert(decoded_apdu.tag.constructed == 0);
    assert(decoded_apdu.tag.tag_number == 0U);
    assert(decoded_apdu.payload_length == sizeof(payload));
    assert(memcmp(decoded_apdu.payload_bytes, payload, sizeof(payload)) == 0);
}

static void test_presentation_simply_encoded_zero_length_payload(void)
{
    uint8_t buffer[16];
    UnitLabMmsPresentationApdu apdu;
    UnitLabMmsPresentationApdu decoded_apdu;
    size_t encoded_length = 123U;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    apdu.payload_bytes = NULL;
    apdu.payload_length = 0U;
    assert(unitlab_mms_presentation_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == 2U);
    assert(buffer[0] == 0x40U);
    assert(buffer[1] == 0x00U);
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_apdu.payload_length == 0U);
}

static void test_presentation_decode_rejects_malformed_pdv_list_shape(void)
{
    const uint8_t buffer[] = { 0x61U, 0x0BU, 0x30U, 0x09U, 0x02U, 0x01U, 0x01U, 0xA0U, 0x05U, 0x30U, 0x02U, 0x01U, 0x01U };
    UnitLabMmsPresentationApdu decoded_apdu;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    decoded_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    decoded_apdu.payload_bytes = (const uint8_t*)0x1;
    decoded_apdu.payload_length = 99U;
    decoded_apdu.encoded_length = 77U;
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(decoded_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_NONE);
    assert(decoded_apdu.payload_bytes == NULL);
    assert(decoded_apdu.payload_length == 0U);
    assert(decoded_apdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL || diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
}

static void test_presentation_decode_reports_trailing_bytes(void)
{
    uint8_t buffer[32];
    UnitLabMmsPresentationApdu apdu;
    UnitLabMmsPresentationApdu decoded_apdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[4] = { 0x30U, 0x02U, 0x01U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    apdu.payload_bytes = payload;
    apdu.payload_length = sizeof(payload);
    assert(unitlab_mms_presentation_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    buffer[encoded_length + 0U] = 0xDEU;
    buffer[encoded_length + 1U] = 0xADU;
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, encoded_length + 2U, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_apdu.payload_length == sizeof(payload));
    assert(memcmp(decoded_apdu.payload_bytes, payload, sizeof(payload)) == 0);
}

static void test_presentation_decode_resets_output_on_failure(void)
{
    uint8_t buffer[32];
    UnitLabMmsPresentationApdu decoded_apdu;
    UnitLabMmsBerElement element;
    size_t encoded_length = 0U;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[2] = { 0x30U, 0x00U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_PRIVATE;
    element.tag.constructed = 0;
    element.tag.tag_number = 7U;
    element.value_bytes = payload;
    element.value_length = sizeof(payload);
    assert(unitlab_mms_ber_write(&element, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    decoded_apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    decoded_apdu.payload_bytes = (const uint8_t*)0x1;
    decoded_apdu.payload_length = 99U;
    decoded_apdu.encoded_length = 77U;
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(decoded_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_NONE);
    assert(decoded_apdu.payload_bytes == NULL);
    assert(decoded_apdu.payload_length == 0U);
    assert(decoded_apdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
    assert(diagnostic.message[0] != '\0');
}

static void test_presentation_fully_encoded_roundtrip(void)
{
    uint8_t buffer[64];
    UnitLabMmsPresentationApdu apdu;
    UnitLabMmsPresentationApdu decoded_apdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[4] = { 0x30U, 0x02U, 0x01U, 0x01U };
    const uint8_t expected_encoding[] = { 0x61U, 0x0BU, 0x30U, 0x09U, 0x02U, 0x01U, 0x01U, 0xA0U, 0x04U, 0x30U, 0x02U, 0x01U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    apdu.payload_bytes = payload;
    apdu.payload_length = sizeof(payload);
    assert(unitlab_mms_presentation_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == sizeof(expected_encoding));
    assert(memcmp(buffer, expected_encoding, sizeof(expected_encoding)) == 0);
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(decoded_apdu.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION);
    assert(decoded_apdu.tag.constructed == 1);
    assert(decoded_apdu.tag.tag_number == 1U);
    assert(decoded_apdu.payload_length == sizeof(payload));
    assert(memcmp(decoded_apdu.payload_bytes, payload, sizeof(payload)) == 0);
}

static void test_presentation_decode_accepts_pdv_list_wrapper(void)
{
    uint8_t buffer[64];
    UnitLabMmsPresentationApdu decoded_apdu;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[4] = { 0x30U, 0x02U, 0x01U, 0x01U };
    const uint8_t expected_encoding[] = { 0x61U, 0x0BU, 0x30U, 0x09U, 0x02U, 0x01U, 0x01U, 0xA0U, 0x04U, 0x30U, 0x02U, 0x01U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    memcpy(buffer, expected_encoding, sizeof(expected_encoding));
    unitlab_mms_presentation_apdu_init(&decoded_apdu);
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, sizeof(expected_encoding), &consumed_length, &diagnostic) == 1);
    assert(consumed_length == sizeof(expected_encoding));
    assert(decoded_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(decoded_apdu.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION);
    assert(decoded_apdu.tag.constructed == 1);
    assert(decoded_apdu.tag.tag_number == 1U);
    assert(decoded_apdu.payload_length == sizeof(payload));
    assert(memcmp(decoded_apdu.payload_bytes, payload, sizeof(payload)) == 0);
}

static void test_acse_decode_accepts_raw_aarq_fields(void)
{
    uint8_t buffer[64];
    UnitLabMmsAcseApdu apdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[] = {
        0xA0U, 0x03U, 0x80U, 0x01U, 0x01U,
        0xA2U, 0x03U, 0x80U, 0x01U, 0x01U,
    };

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_ber_write(&(UnitLabMmsBerElement){ .tag = { UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 0U }, .value_bytes = payload, .value_length = sizeof(payload) }, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    unitlab_mms_acse_apdu_init(&apdu);
    assert(unitlab_mms_acse_decode(&apdu, payload, sizeof(payload), &consumed_length, &diagnostic) == 1);
    assert(consumed_length == sizeof(payload));
    assert(apdu.kind == UNITLAB_MMS_ACSE_APDU_AARQ);
    assert(apdu.field_count >= 2U);
    assert(apdu.fields[0].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(apdu.fields[0].tag.tag_number == 0U);
}

static void test_presentation_decode_rejects_unsupported_outer_tag(void)
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
    assert(unitlab_mms_presentation_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
}

static void test_presentation_encode_rejects_non_supported_kind(void)
{
    uint8_t buffer[16];
    UnitLabMmsPresentationApdu apdu;
    size_t encoded_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_presentation_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_NONE;
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
    apdu.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    apdu.payload_bytes = NULL;
    apdu.payload_length = 1U;
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

static void test_wire_frame_builder_information_report_roundtrip(void)
{
    uint8_t scratch[64];
    uint8_t frame_bytes[128];
    UnitLabMmsPdu decoded_pdu;
    UnitLabMmsAssociationFrame decoded_fixture;
    size_t frame_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_build_information_report_frame("RPT", 0U, scratch, sizeof(scratch), frame_bytes, sizeof(frame_bytes), &frame_length, &diagnostic) == 1);
    assert(frame_length > 20U);
    assert(frame_bytes[0] == 0x03U);
    assert(frame_bytes[4] == 0x02U);
    assert(frame_bytes[5] == 0xF0U);
    assert(frame_bytes[6] == 0x80U);

    unitlab_mms_association_frame_init(&decoded_fixture);
    assert(unitlab_mms_association_frame_decode(&decoded_fixture, frame_bytes, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_fixture.presentation.payload_length > 0U);
    assert(decoded_fixture.presentation.payload_bytes[0] == 0x63U);

    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, decoded_fixture.presentation.payload_bytes, decoded_fixture.presentation.payload_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == decoded_fixture.presentation.payload_length);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_UNCONFIRMED);
    assert(decoded_pdu.has_service == 1);
    assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_INFORMATION_REPORT);
    assert(decoded_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_pdu.service_tag.tag_number == 3U || decoded_pdu.service_tag.tag_number == 0U);
}

static void test_wire_frame_builder_aarq_association_roundtrip(void)
{
    uint8_t frame_bytes[160];
    UnitLabMmsAssociationFrame fixture;
    UnitLabMmsAssociationFrame decoded_fixture;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsBerElement external_element;
    size_t frame_length = 0U;
    size_t consumed_length = 0U;
    size_t external_consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t aarq_payload[67U] = {
        0x60U, 0x41U,
        0x80U, 0x01U, 0x00U,
        0xA1U, 0x07U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x12U, 0x02U, 0x03U,
        0xBEU, 0x33U,
        0x28U, 0x31U,
        0x06U, 0x02U, 0x52U, 0x01U,
        0x02U, 0x01U, 0x03U,
        0xA0U, 0x28U,
        0xA9U, 0x26U,
        0x80U, 0x03U, 0x00U, 0xFAU, 0x00U,
        0x81U, 0x01U, 0x0AU,
        0x82U, 0x01U, 0x0AU,
        0x83U, 0x01U, 0x05U,
        0xA4U, 0x16U,
        0x80U, 0x01U, 0x01U,
        0x81U, 0x03U, 0x05U, 0xE1U, 0x00U,
        0x82U, 0x0CU, 0x03U, 0xA0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0xEDU, 0x10U,
    };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_association_frame_init(&fixture);
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    fixture.presentation.payload_bytes = aarq_payload;
    fixture.presentation.payload_length = sizeof(aarq_payload);
    assert(unitlab_mms_association_frame_encode(&fixture, frame_bytes, sizeof(frame_bytes), &frame_length, &diagnostic) == 1);
    assert(frame_length >= 32U);
    assert(frame_bytes[0] == 0x03U);
    assert(frame_bytes[1] == 0x00U);
    assert(frame_bytes[4] == 0x02U);
    assert(frame_bytes[5] == 0xF0U);
    assert(frame_bytes[6] == 0x80U);
    assert(frame_bytes[7] == 0x01U);
    assert(frame_bytes[8] == 0x00U);
    assert(frame_bytes[9] == 0x01U);
    assert(frame_bytes[10] == 0x00U);
    assert(frame_bytes[11] == 0x40U);
    assert(frame_bytes[12] == 0x43U);
    assert(frame_bytes[13] == 0x60U);
    assert(frame_bytes[14] == 0x41U);
    assert(frame_bytes[15] == 0x80U);
    assert(frame_bytes[16] == 0x01U);
    assert(frame_bytes[17] == 0x00U);
    assert(frame_bytes[18] == 0xA1U);
    assert(frame_bytes[19] == 0x07U);
    assert(frame_bytes[20] == 0x06U);
    assert(frame_bytes[21] == 0x05U);
    assert(frame_bytes[22] == 0x28U);
    assert(frame_bytes[23] == 0xCAU);
    assert(frame_bytes[24] == 0x12U);
    assert(frame_bytes[25] == 0x02U);
    assert(frame_bytes[26] == 0x03U);
    assert(frame_bytes[27] == 0xBEU);
    assert(frame_bytes[28] == 0x33U);

    unitlab_mms_association_frame_init(&decoded_fixture);
    assert(unitlab_mms_association_frame_decode(&decoded_fixture, frame_bytes, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(decoded_fixture.presentation.payload_length == sizeof(aarq_payload));
    assert(memcmp(decoded_fixture.presentation.payload_bytes, aarq_payload, sizeof(aarq_payload)) == 0);

    unitlab_mms_acse_apdu_init(&acse_apdu);
    assert(unitlab_mms_acse_decode(&acse_apdu, decoded_fixture.presentation.payload_bytes, decoded_fixture.presentation.payload_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == sizeof(aarq_payload));
    assert(acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARQ);
    assert(acse_apdu.field_count == 3U);
    assert(acse_apdu.fields[0].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[0].tag.tag_number == 0U);
    assert(acse_apdu.fields[1].tag.tag_number == 1U);
    assert(acse_apdu.fields[2].tag.tag_number == 30U);
    assert(acse_apdu.fields[1].value_length == 7U);
    assert(acse_apdu.fields[1].value_bytes[0] == 0x06U);
    assert(acse_apdu.fields[1].value_bytes[1] == 0x05U);
    assert(acse_apdu.fields[1].value_bytes[2] == 0x28U);
    assert(acse_apdu.fields[1].value_bytes[3] == 0xCAU);
    assert(acse_apdu.fields[1].value_bytes[4] == 0x12U);
    assert(acse_apdu.fields[1].value_bytes[5] == 0x02U);
    assert(acse_apdu.fields[1].value_bytes[6] == 0x03U);

    unitlab_mms_ber_element_init(&external_element);
    assert(unitlab_mms_ber_read(&external_element, acse_apdu.fields[2].value_bytes, acse_apdu.fields[2].value_length, &external_consumed_length, &diagnostic) == 1);
    assert(external_consumed_length == acse_apdu.fields[2].value_length);
    assert(external_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL);
    assert(external_element.tag.tag_number == 8U);
    assert(external_element.value_length > 11U);
    assert(external_element.value_bytes[0] == 0x06U);
    assert(external_element.value_bytes[1] == 0x02U);
    assert(external_element.value_bytes[2] == 0x52U);
    assert(external_element.value_bytes[3] == 0x01U);
    assert(external_element.value_bytes[4] == 0x02U);
    assert(external_element.value_bytes[5] == 0x01U);
    assert(external_element.value_bytes[6] == 0x03U);
    assert(external_element.value_bytes[7] == 0xA0U);
    assert(external_element.value_bytes[8] == 0x28U);
    assert(external_element.value_bytes[9] == 0xA9U);
    assert(external_element.value_bytes[10] == 0x26U);

    assert(memcmp(&external_element.value_bytes[9], (uint8_t[]){
        0xA9U, 0x26U,
        0x80U, 0x03U, 0x00U, 0xFAU, 0x00U,
        0x81U, 0x01U, 0x0AU,
        0x82U, 0x01U, 0x0AU,
        0x83U, 0x01U, 0x05U,
        0xA4U, 0x16U,
        0x80U, 0x01U, 0x01U,
        0x81U, 0x03U, 0x05U, 0xE1U, 0x00U,
        0x82U, 0x0CU, 0x03U, 0xA0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U, 0xEDU, 0x10U,
    }, 40U) == 0);
}

// static void test_association_response_frame_roundtrip(void)
// {
//     uint8_t frame[256];
//     UnitLabMmsAssociationFrame decoded_fixture;
//     UnitLabMmsAcseApdu acse_apdu;
//     size_t frame_length = 0U;
//     size_t consumed_length = 0U;
//     size_t acse_consumed_length = 0U;
//     UnitLabMmsDiagnostic diagnostic;

//     unitlab_mms_diagnostic_clear(&diagnostic);
//     assert(unitlab_mms_build_association_response_frame(frame, sizeof(frame), &frame_length, &diagnostic) == 1);
//     assert(frame_length > 0U);
//     /* Association response must not be sent as ordinary Session DATA TRANSFER. */
//     assert(frame[0] == 0x03U);
//     assert(frame[1] == 0x00U);
//     assert(frame[4] == 0x02U);
//     assert(frame[5] == 0xF0U);
//     assert(frame[6] == 0x80U);

//     /*
//     * Session ACCEPT SPDU smoke path.
//     * Old wrong behavior was:
//     *   01 00 01 00 40 ...
//     * which means Give Tokens + DATA TRANSFER + Presentation simply-encoded-data.
//     *
//     * Association response must no longer start with the ordinary DT sequence.
//     */
//     assert(frame[7] == 0x0EU);
//     assert(!(frame[7] == 0x01U && frame[8] == 0x00U && frame[9] == 0x01U && frame[10] == 0x00U));

//     /* AARE bytes must still be present in the association response. */
//     int found_aare = 0;
//     for (size_t i = 0U; i + 1U < frame_length; i++) {
//         if (frame[i] == 0x61U && frame[i + 1U] == 0x4AU) {
//             found_aare = 1;
//             break;
//         }
//     }
//     assert(found_aare == 1);

//     unitlab_mms_association_frame_init(&decoded_fixture);
//     assert(unitlab_mms_association_frame_decode(&decoded_fixture, frame, frame_length, &consumed_length, &diagnostic) == 1);
//     assert(consumed_length == frame_length);
//     assert(decoded_fixture.session.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
//     assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
//     assert(decoded_fixture.presentation.payload_length > 0U);
//     assert(decoded_fixture.presentation.payload_bytes[0] == 0x61U);

//     unitlab_mms_acse_apdu_init(&acse_apdu);
//     assert(unitlab_mms_acse_decode(&acse_apdu, decoded_fixture.presentation.payload_bytes, decoded_fixture.presentation.payload_length, &acse_consumed_length, &diagnostic) == 1);
//     assert(acse_consumed_length == decoded_fixture.presentation.payload_length);
//     assert(acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARE);
//     assert(acse_apdu.field_count == 4U);
//     assert(acse_apdu.fields[0].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
//     assert(acse_apdu.fields[0].tag.tag_number == 1U);
//     assert(acse_apdu.fields[1].tag.tag_number == 2U);
//     assert(acse_apdu.fields[2].tag.tag_number == 3U);
//     assert(acse_apdu.fields[3].tag.tag_number == 30U);
// }

static void test_initiate_response_profile_uses_model_plan(void)
{
    UnitLabMmsInitiateResponseProfile profile;
    UnitLabIedModelPlan plan;
    UnitLabIedModelSignal signals[2U];

    memset(&profile, 0, sizeof(profile));
    memset(&plan, 0, sizeof(plan));
    memset(signals, 0, sizeof(signals));

    strcpy(signals[0].object_reference, "XCBR1.Pos.stVal");
    strcpy(signals[1].object_reference, "MMXU1.A.phsA.cVal.mag.f");
    plan.logical_device_count = 1U;
    plan.data_set_count = 1U;
    plan.report_count = 1U;
    plan.signal_count = 2U;
    plan.signals = signals;

    unitlab_mms_initiate_response_profile_init(&profile);
    unitlab_mms_initiate_response_profile_apply_model_plan(&profile, &plan);

    assert(profile.local_detail_called == 8000U);
    assert(profile.max_serv_outstanding_calling == 1U);
    assert(profile.max_serv_outstanding_called == 1U);
    assert(profile.data_structure_nesting_level == 8U);
}

static void test_initiate_response_profile_scales_with_larger_model(void)
{
    UnitLabMmsInitiateResponseProfile profile;
    UnitLabIedModelPlan plan;
    UnitLabIedModelSignal signals[17U];

    memset(&profile, 0, sizeof(profile));
    memset(&plan, 0, sizeof(plan));
    memset(signals, 0, sizeof(signals));

    strcpy(signals[0].object_reference, "XCBR1.Pos.stVal");
    strcpy(signals[16].object_reference, "MMXU1.A.phsA.cVal.mag.f");
    plan.logical_device_count = 2U;
    plan.data_set_count = 3U;
    plan.report_count = 5U;
    plan.signal_count = 17U;
    plan.signals = signals;

    unitlab_mms_initiate_response_profile_init(&profile);
    unitlab_mms_initiate_response_profile_apply_model_plan(&profile, &plan);

    assert(profile.local_detail_called == 10560U);
    assert(profile.max_serv_outstanding_calling == 3U);
    assert(profile.max_serv_outstanding_called == 3U);
    assert(profile.data_structure_nesting_level == 8U);
}

static void test_association_response_frame_smoke(void)
{
    uint8_t frame[512];
    UnitLabMmsTransportFrame decoded_frame;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsBerElement field_element;
    UnitLabMmsBerElement inner_element;
    size_t frame_length = 0U;
    size_t consumed_length = 0U;
    size_t acse_consumed_length = 0U;
    size_t field_consumed_length = 0U;
    size_t inner_consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t expected_oid[] = { 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U };
    const uint8_t expected_version[] = { 0x07U, 0x80U };

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(unitlab_mms_build_association_response_frame(
        frame,
        sizeof(frame),
        &frame_length,
        &diagnostic) == 1);

    unitlab_mms_transport_frame_init(&decoded_frame);
    assert(unitlab_mms_transport_frame_decode(&decoded_frame, frame, frame_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == frame_length);
    assert(decoded_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT);
    assert(decoded_frame.cotp.eot == 1);
    assert(decoded_frame.cotp.user_data_length > 0U);

    unitlab_mms_session_spdu_init(&session_spdu);
    assert(unitlab_mms_session_spdu_decode(&session_spdu, decoded_frame.cotp.user_data, decoded_frame.cotp.user_data_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == decoded_frame.cotp.user_data_length);
    assert(session_spdu.kind == UNITLAB_MMS_SESSION_SPDU_ACCEPT);
    assert(session_spdu.raw_parameter_length > 0U);

    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    assert(unitlab_mms_presentation_decode(&presentation_apdu, session_spdu.raw_parameter_bytes, session_spdu.raw_parameter_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == session_spdu.raw_parameter_length);
    assert(presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(presentation_apdu.context_identifier == 1U);
    assert(presentation_apdu.payload_length > 0U);
    assert(presentation_apdu.payload_bytes[0] == 0x61U);

    unitlab_mms_acse_apdu_init(&acse_apdu);
    assert(unitlab_mms_acse_decode(&acse_apdu, presentation_apdu.payload_bytes, presentation_apdu.payload_length, &acse_consumed_length, &diagnostic) == 1);
    assert(acse_consumed_length == presentation_apdu.payload_length);
    assert(acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARE);
    assert(acse_apdu.apdu_length > 0U);
    assert(acse_apdu.apdu_bytes != NULL);
    assert(acse_apdu.apdu_bytes[0] == 0x30U);
    assert(acse_apdu.field_count == 5U);

    assert(acse_apdu.fields[0].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[0].tag.tag_number == 0U);
    assert(acse_apdu.fields[0].tag.constructed == 0);
    assert(acse_apdu.fields[0].value_length == sizeof(expected_version));
    assert(memcmp(acse_apdu.fields[0].value_bytes, expected_version, sizeof(expected_version)) == 0);

    assert(acse_apdu.fields[1].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[1].tag.tag_number == 1U);
    assert(acse_apdu.fields[1].tag.constructed == 0);
    assert(acse_apdu.fields[1].value_length == sizeof(expected_oid));
    assert(memcmp(acse_apdu.fields[1].value_bytes, expected_oid, sizeof(expected_oid)) == 0);

    assert(acse_apdu.fields[2].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[2].tag.tag_number == 2U);
    assert(acse_apdu.fields[2].tag.constructed == 0);
    assert(acse_apdu.fields[2].value_length == 1U);
    assert(acse_apdu.fields[2].value_bytes[0] == 0x00U);

    assert(acse_apdu.fields[3].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[3].tag.tag_number == 3U);
    assert(acse_apdu.fields[3].tag.constructed == 1);
    unitlab_mms_ber_element_init(&inner_element);
    assert(unitlab_mms_ber_read(&inner_element, acse_apdu.fields[3].value_bytes, acse_apdu.fields[3].value_length, &inner_consumed_length, &diagnostic) == 1);
    assert(inner_consumed_length == acse_apdu.fields[3].value_length);
    assert(inner_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(inner_element.tag.tag_number == 1U);
    assert(inner_element.value_length == 1U);
    assert(inner_element.value_bytes[0] == 0x00U);

    assert(acse_apdu.fields[4].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[4].tag.tag_number == 30U);
    assert(acse_apdu.fields[4].tag.constructed == 1);
    unitlab_mms_ber_element_init(&field_element);
    assert(unitlab_mms_ber_read(&field_element, acse_apdu.fields[4].value_bytes, acse_apdu.fields[4].value_length, &field_consumed_length, &diagnostic) == 1);
    assert(field_consumed_length == acse_apdu.fields[4].value_length);
    assert(field_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL);
    assert(field_element.tag.tag_number == 8U);
    assert(field_element.tag.constructed == 1);
    assert(field_element.value_length > 0U);
}

static void test_acse_association_accept_frame_roundtrip(void)
{
    uint8_t initiate_response_detail[4U] = { 0x30U, 0x02U, 0x01U, 0x01U };
    uint8_t initiate_response_bytes[32U];
    uint8_t acse_bytes[256U];
    UnitLabMmsPdu initiate_response_pdu;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsBerElement field_element;
    UnitLabMmsBerElement inner_element;
    size_t initiate_response_length = 0U;
    size_t acse_length = 0U;
    size_t consumed_length = 0U;
    size_t field_consumed_length = 0U;
    size_t inner_consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t expected_oid[] = { 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U };
    const uint8_t expected_version[] = { 0x07U, 0x80U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&initiate_response_pdu);
    initiate_response_pdu.kind = UNITLAB_MMS_PDU_INITIATE_RESPONSE;
    initiate_response_pdu.pdu_bytes = initiate_response_detail;
    initiate_response_pdu.pdu_length = sizeof(initiate_response_detail);
    assert(unitlab_mms_pdu_encode(&initiate_response_pdu, initiate_response_bytes, sizeof(initiate_response_bytes), &initiate_response_length, &diagnostic) == 1);
    assert(unitlab_mms_acse_build_association_accept_frame(initiate_response_bytes, initiate_response_length, acse_bytes, sizeof(acse_bytes), &acse_length, &diagnostic) == 1);

    unitlab_mms_acse_apdu_init(&acse_apdu);
    assert(unitlab_mms_acse_decode(&acse_apdu, acse_bytes, acse_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == acse_length);
    assert(acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARE);
    assert(acse_apdu.field_count == 5U);

    assert(acse_apdu.fields[0].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[0].tag.tag_number == 0U);
    assert(acse_apdu.fields[0].tag.constructed == 0);
    assert(acse_apdu.fields[0].value_length == sizeof(expected_version));
    assert(memcmp(acse_apdu.fields[0].value_bytes, expected_version, sizeof(expected_version)) == 0);

    assert(acse_apdu.fields[1].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[1].tag.tag_number == 1U);
    assert(acse_apdu.fields[1].tag.constructed == 0);
    assert(acse_apdu.fields[1].value_length == sizeof(expected_oid));
    assert(memcmp(acse_apdu.fields[1].value_bytes, expected_oid, sizeof(expected_oid)) == 0);

    assert(acse_apdu.fields[2].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[2].tag.tag_number == 2U);
    assert(acse_apdu.fields[2].tag.constructed == 0);
    assert(acse_apdu.fields[2].value_length == 1U);
    assert(acse_apdu.fields[2].value_bytes[0] == 0x00U);

    assert(acse_apdu.fields[3].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[3].tag.tag_number == 3U);
    assert(acse_apdu.fields[3].tag.constructed == 1);
    unitlab_mms_ber_element_init(&inner_element);
    assert(unitlab_mms_ber_read(&inner_element, acse_apdu.fields[3].value_bytes, acse_apdu.fields[3].value_length, &inner_consumed_length, &diagnostic) == 1);
    assert(inner_consumed_length == acse_apdu.fields[3].value_length);
    assert(inner_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(inner_element.tag.tag_number == 1U);
    assert(inner_element.value_length == 1U);
    assert(inner_element.value_bytes[0] == 0x00U);

    assert(acse_apdu.fields[4].tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(acse_apdu.fields[4].tag.tag_number == 30U);
    assert(acse_apdu.fields[4].tag.constructed == 1);
    unitlab_mms_ber_element_init(&field_element);
    assert(unitlab_mms_ber_read(&field_element, acse_apdu.fields[4].value_bytes, acse_apdu.fields[4].value_length, &field_consumed_length, &diagnostic) == 1);
    assert(field_consumed_length == acse_apdu.fields[4].value_length);
    assert(field_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL);
    assert(field_element.tag.tag_number == 8U);
    assert(field_element.tag.constructed == 1);
    assert(field_element.value_length > 0U);
}

static void test_mms_pdu_initiate_roundtrip(void)
{
    struct {
        UnitLabMmsPduKind kind;
        uint8_t tag;
    } cases[] = {
        { UNITLAB_MMS_PDU_INITIATE_REQUEST, 0x68U },
        { UNITLAB_MMS_PDU_INITIATE_RESPONSE, 0x69U },
        { UNITLAB_MMS_PDU_INITIATE_ERROR, 0x6AU },
    };
    uint8_t buffer[32];
    UnitLabMmsPdu pdu;
    UnitLabMmsPdu decoded_pdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[4] = { 0x80U, 0x01U, 0x00U, 0x00U };

    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        unitlab_mms_diagnostic_clear(&diagnostic);
        unitlab_mms_pdu_init(&pdu);
        pdu.kind = cases[i].kind;
        pdu.pdu_bytes = payload;
        pdu.pdu_length = sizeof(payload);
        assert(unitlab_mms_pdu_encode(&pdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
        assert(buffer[0] == cases[i].tag);
        unitlab_mms_pdu_init(&decoded_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
        assert(consumed_length == encoded_length);
        assert(decoded_pdu.kind == cases[i].kind);
        assert(decoded_pdu.pdu_length > 0U);
        }
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

static void test_session_spdu_roundtrip_long_payload(void)
{
    uint8_t buffer[400];
    UnitLabMmsSessionSpdu spdu;
    UnitLabMmsSessionSpdu decoded_spdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    uint8_t payload[260U];

    for (size_t i = 0U; i < 260U; i++) {
        payload[i] = (uint8_t)(i & 0xFFU);
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    spdu.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    spdu.spdu_bytes = payload;
    spdu.spdu_length = sizeof(payload);
    assert(unitlab_mms_session_spdu_encode(&spdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == sizeof(payload) + 4U);
    assert(buffer[0] == 1U);
    assert(buffer[1] == 0U);
    assert(buffer[2] == 1U);
    assert(buffer[3] == 0U);
    unitlab_mms_session_spdu_init(&decoded_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_spdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_spdu.kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER);
    assert(decoded_spdu.spdu_length == encoded_length);
    assert(decoded_spdu.raw_parameter_length == sizeof(payload));
    assert(memcmp(decoded_spdu.raw_parameter_bytes, &buffer[4], sizeof(payload)) == 0);
}

static void test_session_spdu_decode_stops_at_indicated_length(void)
{
    uint8_t buffer[16];
    UnitLabMmsSessionSpdu spdu;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[3] = { 0xAAU, 0xBBU, 0xCCU };

    buffer[0] = 1U;
    buffer[1] = 0U;
    buffer[2] = 1U;
    buffer[3] = 0U;
    memcpy(&buffer[4], payload, sizeof(payload));

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    assert(unitlab_mms_session_spdu_decode(&spdu, buffer, sizeof(payload) + 4U, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == sizeof(payload) + 4U);
    assert(spdu.spdu_length == sizeof(payload) + 4U);
    assert(spdu.raw_parameter_length == sizeof(payload));
    assert(memcmp(spdu.raw_parameter_bytes, payload, sizeof(payload)) == 0);
}

static void test_session_spdu_rejects_truncated_data_transfer(void)
{
    const uint8_t buffer[3] = { 0x01U, 0x00U, 0x01U };
    UnitLabMmsSessionSpdu spdu;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    assert(unitlab_mms_session_spdu_decode(&spdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(spdu.kind == UNITLAB_MMS_SESSION_SPDU_NONE);
    assert(spdu.spdu_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
}

static void test_session_spdu_rejects_invalid_data_transfer_header(void)
{
    const uint8_t buffer[4] = { 0x01U, 0x01U, 0x01U, 0x00U };
    UnitLabMmsSessionSpdu spdu;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    assert(unitlab_mms_session_spdu_decode(&spdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(spdu.kind == UNITLAB_MMS_SESSION_SPDU_NONE);
    assert(spdu.spdu_length == 0U);
    assert(spdu.raw_parameter_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
}

static void test_session_spdu_rejects_unsupported_kind(void)
{
    uint8_t buffer[8];
    UnitLabMmsSessionSpdu spdu;
    size_t encoded_length = 123U;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    spdu.kind = UNITLAB_MMS_SESSION_SPDU_NONE;
    spdu.spdu_bytes = buffer;
    spdu.spdu_length = 1U;
    assert(unitlab_mms_session_spdu_encode(&spdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 0);
    assert(encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
    assert(diagnostic.message[0] != '\0');

    unitlab_mms_session_spdu_init(&spdu);
    buffer[0] = 0xFFU;
    buffer[1] = 0x00U;
    assert(unitlab_mms_session_spdu_decode(&spdu, buffer, 2U, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(spdu.kind == UNITLAB_MMS_SESSION_SPDU_NONE);
    assert(spdu.spdu_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
    assert(diagnostic.message[0] != '\0');
}

static void test_session_spdu_decode_reports_consumed_length_with_trailing_bytes(void)
{
    uint8_t buffer[32];
    UnitLabMmsSessionSpdu spdu;
    UnitLabMmsSessionSpdu decoded_spdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[5] = { 0xA1U, 0xB2U, 0xC3U, 0xD4U, 0xE5U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    spdu.kind = UNITLAB_MMS_SESSION_SPDU_ACCEPT;
    spdu.spdu_bytes = payload;
    spdu.spdu_length = sizeof(payload);
    assert(unitlab_mms_session_spdu_encode(&spdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    buffer[encoded_length + 0U] = 0xDEU;
    buffer[encoded_length + 1U] = 0xADU;
    unitlab_mms_session_spdu_init(&decoded_spdu);
    assert(unitlab_mms_session_spdu_decode(&decoded_spdu, buffer, encoded_length + 2U, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_spdu.spdu_length == encoded_length);
    assert(decoded_spdu.raw_parameter_length == sizeof(payload));
    assert(memcmp(decoded_spdu.raw_parameter_bytes, payload, sizeof(payload)) == 0);
}

static void test_session_spdu_decode_resets_output_on_failure(void)
{
    const uint8_t buffer[2] = { 0xFFU, 0x00U };
    UnitLabMmsSessionSpdu spdu;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_spdu_init(&spdu);
    spdu.kind = UNITLAB_MMS_SESSION_SPDU_ACCEPT;
    spdu.spdu_bytes = (const uint8_t*)0x1;
    spdu.spdu_length = 99U;
    spdu.raw_parameter_bytes = (const uint8_t*)0x2;
    spdu.raw_parameter_length = 88U;
    spdu.encoded_length = 77U;
    assert(unitlab_mms_session_spdu_decode(&spdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(spdu.kind == UNITLAB_MMS_SESSION_SPDU_NONE);
    assert(spdu.spdu_bytes == NULL);
    assert(spdu.spdu_length == 0U);
    assert(spdu.raw_parameter_bytes == NULL);
    assert(spdu.raw_parameter_length == 0U);
    assert(spdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
    assert(diagnostic.message[0] != '\0');
}

static void test_session_spdu_roundtrips(void)
{
    struct {
        UnitLabMmsSessionSpduKind kind;
        uint8_t code;
        const uint8_t payload[5];
        size_t payload_length;
    } cases[] = {
        { UNITLAB_MMS_SESSION_SPDU_CONNECT, 13U, { 13U, 3U, 0xC1U, 1U, 0xA0U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_CONNECT_DATA_OVERFLOW, 15U, { 15U, 3U, 0xAAU, 0xBBU, 0xCCU }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_OVERFLOW_ACCEPT, 16U, { 16U, 3U, 0x11U, 0x22U, 0x33U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_ACCEPT, 14U, { 0xA1U, 0xB2U, 0xC3U, 0xD4U, 0xE5U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_REFUSE, 12U, { 12U, 3U, 0x77U, 0x88U, 0x99U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_FINISH, 9U, { 9U, 3U, 0x10U, 0x20U, 0x30U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_DISCONNECT, 10U, { 10U, 3U, 0x40U, 0x50U, 0x60U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_ABORT, 25U, { 25U, 3U, 0x70U, 0x80U, 0x90U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_ABORT_ACCEPT, 26U, { 26U, 0U, 0x00U, 0x00U, 0x00U }, 2U },
        { UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER, 1U, { 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_EXPEDITED_DATA, 5U, { 5U, 3U, 0x02U, 0x03U, 0x04U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_TYPED_DATA, 33U, { 33U, 3U, 0x05U, 0x06U, 0x07U }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA, 61U, { 61U, 3U, 0x08U, 0x09U, 0x0AU }, 5U },
        { UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA_ACK, 62U, { 62U, 3U, 0x0BU, 0x0CU, 0x0DU }, 5U },
    };
    uint8_t buffer[64];
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
        if (cases[i].kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER) {
            assert(encoded_length == cases[i].payload_length + 4U);
            assert(buffer[0] == cases[i].code);
            assert(buffer[1] == 0U);
            assert(buffer[2] == cases[i].code);
            assert(buffer[3] == 0U);
        } else if (cases[i].kind == UNITLAB_MMS_SESSION_SPDU_ACCEPT) {
            assert(encoded_length == cases[i].payload_length + 20U);
            assert(buffer[0] == cases[i].code);
            assert(buffer[1] == (uint8_t)(18U + cases[i].payload_length));
            assert(buffer[2] == 0x05U);
            assert(buffer[10] == 0x14U);
            assert(buffer[14] == 0x34U);
            assert(buffer[18] == 0xC1U);
        } else {
            assert(encoded_length == cases[i].payload_length);
            assert(buffer[0] == cases[i].code);
        }
        unitlab_mms_session_spdu_init(&decoded_spdu);
        assert(unitlab_mms_session_spdu_decode(&decoded_spdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
        assert(consumed_length == encoded_length);
        assert(decoded_spdu.kind == cases[i].kind);
        if (cases[i].kind == UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER) {
            assert(decoded_spdu.spdu_length == encoded_length);
            assert(decoded_spdu.raw_parameter_length == cases[i].payload_length);
            assert(decoded_spdu.raw_parameter_bytes == &decoded_spdu.spdu_bytes[4]);
            assert(memcmp(decoded_spdu.raw_parameter_bytes, cases[i].payload, cases[i].payload_length) == 0);
        } else if (cases[i].kind == UNITLAB_MMS_SESSION_SPDU_ACCEPT) {
            assert(decoded_spdu.spdu_length == encoded_length);
            assert(decoded_spdu.raw_parameter_length == cases[i].payload_length);
            assert(memcmp(decoded_spdu.raw_parameter_bytes, cases[i].payload, cases[i].payload_length) == 0);
        } else if (cases[i].kind == UNITLAB_MMS_SESSION_SPDU_CONNECT) {
            assert(decoded_spdu.spdu_length == cases[i].payload_length);
            assert(decoded_spdu.raw_parameter_length == 1U);
            assert(decoded_spdu.raw_parameter_bytes == &decoded_spdu.spdu_bytes[4]);
            assert(decoded_spdu.raw_parameter_bytes[0] == cases[i].payload[4]);
        } else {
            assert(decoded_spdu.spdu_length == cases[i].payload_length);
            assert(decoded_spdu.raw_parameter_length == cases[i].payload_length - 2U);
            assert(decoded_spdu.raw_parameter_bytes == &decoded_spdu.spdu_bytes[2]);
            assert(memcmp(decoded_spdu.spdu_bytes, cases[i].payload, cases[i].payload_length) == 0);
        }
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
    spdu.kind = UNITLAB_MMS_SESSION_SPDU_REFUSE;
    spdu.spdu_bytes = payload;
    spdu.spdu_length = sizeof(payload);
    assert(unitlab_mms_session_spdu_encode(&spdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}

static void test_acse_decode_stops_at_indicated_length(void)
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
    buffer[encoded_length + 0U] = 0xDEU;
    buffer[encoded_length + 1U] = 0xADU;
    buffer[encoded_length + 2U] = 0xBEU;
    buffer[encoded_length + 3U] = 0xEFU;
    unitlab_mms_acse_apdu_init(&decoded_apdu);
    assert(unitlab_mms_acse_decode(&decoded_apdu, buffer, encoded_length + 4U, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARQ);
    assert(decoded_apdu.field_count == 3U);
    assert(decoded_apdu.apdu_length == sizeof(payload));
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
    assert(decoded_pdu.pdu_length > 0U);
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
    assert(decoded_pdu.pdu_length > 0U);
}

static void test_mms_pdu_decode_stops_at_indicated_length(void)
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
    buffer[encoded_length + 0U] = 0xDEU;
    buffer[encoded_length + 1U] = 0xADU;
    buffer[encoded_length + 2U] = 0xBEU;
    buffer[encoded_length + 3U] = 0xEFU;
    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, encoded_length + 4U, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
    assert(decoded_pdu.has_invoke_id == 1);
    assert(decoded_pdu.invoke_id == 5U);
    assert(decoded_pdu.has_service == 1);
    assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
    assert(decoded_pdu.service_length == 1U);
    assert(decoded_pdu.service_bytes[0] == 0xAAU);
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
    assert(decoded_pdu.service_tag.tag_number == 0U || decoded_pdu.service_tag.tag_number == 3U);
    assert(decoded_pdu.service_length == 1U);
    assert(decoded_pdu.service_bytes[0] == 0xAAU);
    assert(decoded_pdu.pdu_length > 0U);
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
    assert(decoded_pdu.pdu_length > 0U);
}

static void test_mms_confirmed_request_roundtrip_with_allocated_invoke_id(void)
{
    uint8_t scratch[128];
    uint8_t frame[256];
    UnitLabMmsSession session;
    UnitLabMmsPdu request_pdu;
    UnitLabMmsPdu decoded_pdu;
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsBerElement presentation_element;
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsDiagnostic diagnostic;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    size_t invoke_id_consumed_length = 0U;
    size_t service_consumed_length = 0U;
    uint32_t invoke_id;
    const uint8_t payload[6] = { 0x02U, 0x01U, 0x01U, 0xA4U, 0x01U, 0x11U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_init(&session);
    invoke_id = unitlab_mms_session_next_invoke_id(&session);
    assert(invoke_id == 1U);

    unitlab_mms_pdu_init(&request_pdu);
    request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    request_pdu.pdu_bytes = payload;
    request_pdu.pdu_length = sizeof(payload);
    assert(unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, sizeof(scratch), frame, sizeof(frame), &encoded_length, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(encoded_length > 0U);

    unitlab_mms_association_frame_init(&association_frame);
    assert(unitlab_mms_association_frame_decode(&association_frame, frame, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(association_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);

    unitlab_mms_ber_element_init(&presentation_element);
    assert(unitlab_mms_ber_read(&presentation_element, association_frame.session.raw_parameter_bytes, association_frame.session.raw_parameter_length, &consumed_length, &diagnostic) == 1);
    assert(presentation_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION);
    assert(presentation_element.tag.tag_number == 1U);
    assert(presentation_element.tag.constructed == 1);

    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed_length, &diagnostic) == 1);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
    assert(decoded_pdu.pdu_length > 0U);

    unitlab_mms_ber_element_init(&invoke_id_element);
    assert(unitlab_mms_ber_read(&invoke_id_element, decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, &invoke_id_consumed_length, &diagnostic) == 1);
    assert(invoke_id_consumed_length > 0U);
    assert(invoke_id_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL);
    assert(invoke_id_element.tag.tag_number == 2U);
    assert(invoke_id_element.value_length == 1U);
    assert(invoke_id_element.value_bytes[0] == (uint8_t)invoke_id);

    unitlab_mms_ber_element_init(&service_element);
    assert(unitlab_mms_ber_read(&service_element, decoded_pdu.pdu_bytes + invoke_id_consumed_length, decoded_pdu.pdu_length - invoke_id_consumed_length, &service_consumed_length, &diagnostic) == 1);
    assert(service_consumed_length > 0U);
    assert(service_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(service_element.tag.tag_number == 4U);
    assert(service_element.tag.constructed == 1);
}

static void test_mms_read_request_wire_frame_builder_roundtrip(void)
{
    uint8_t scratch[256];
    uint8_t frame[256];
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsPdu decoded_pdu;
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsDecodeDiagnostic bridge_diagnostic;
    UnitLabMmsDiagnostic diagnostic;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    size_t invoke_consumed_length = 0U;
    size_t service_consumed_length = 0U;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_pdu_init(&decoded_pdu);
    unitlab_mms_association_frame_init(&association_frame);

    assert(unitlab_mms_build_read_request_frame("XCBR1", "ST$Pos$stVal", 3U, scratch, sizeof(scratch), frame, sizeof(frame), &encoded_length, &diagnostic) == 1);
    assert(encoded_length > 0U);
    assert(unitlab_mms_association_frame_decode(&association_frame, frame, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);

    assert(unitlab_mms_pdu_decode(&decoded_pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed_length, &diagnostic) == 1);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
    assert(decoded_pdu.pdu_length > 0U);

    unitlab_mms_ber_element_init(&invoke_id_element);
    assert(unitlab_mms_ber_read(&invoke_id_element, decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, &invoke_consumed_length, &diagnostic) == 1);
    assert(invoke_id_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL);
    assert(invoke_id_element.tag.tag_number == 2U);
    assert(invoke_id_element.value_length == 1U);
    assert(invoke_id_element.value_bytes[0] == 3U);

    unitlab_mms_ber_element_init(&service_element);
    assert(unitlab_mms_ber_read(&service_element, decoded_pdu.pdu_bytes + invoke_consumed_length, decoded_pdu.pdu_length - invoke_consumed_length, &service_consumed_length, &diagnostic) == 1);
    assert(service_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(service_element.tag.tag_number == 4U);
    assert(service_element.tag.constructed == 1);

    unitlab_mms_pdu_init(&decoded_pdu);
    decoded_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    decoded_pdu.has_invoke_id = 1;
    decoded_pdu.invoke_id = 3U;
    decoded_pdu.has_service = 1;
    decoded_pdu.service_kind = UNITLAB_MMS_SERVICE_READ;
    decoded_pdu.service_bytes = service_element.value_bytes;
    decoded_pdu.service_length = service_element.value_length;
    assert(unitlab_mms_semantic_result_from_wire_pdu(&semantic_result, &decoded_pdu, &bridge_diagnostic) == 1);
    assert(semantic_result.ok == 1);
    assert(semantic_result.pdu.kind == UNITLAB_MMS_DECODED_PDU_READ_REQUEST);
    assert(strcmp(semantic_result.pdu.domain_id, "XCBR1") == 0);
    assert(strcmp(semantic_result.pdu.item_id, "ST$Pos$stVal") == 0);
    assert(strcmp(semantic_result.pdu.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(strcmp(semantic_result.pdu.attribute_reference, "stVal") == 0);
}

static void test_mms_write_request_wire_frame_builder_roundtrip(void)
{
    uint8_t scratch[256];
    uint8_t frame[256];
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsPdu decoded_pdu;
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsDecodeDiagnostic bridge_diagnostic;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement data_element;
    uint8_t boolean_value = 0xFFU;
    const uint8_t expected_payload[] = {
        0x02U, 0x01U, 0x09U,
        0xA5U, 0x24U,
        0x30U, 0x22U,
        0xA0U, 0x1BU,
        0x30U, 0x19U,
        0xA0U, 0x17U,
        0xA1U, 0x15U,
        0x1AU, 0x05U, 'X', 'C', 'B', 'R', '1',
        0x1AU, 0x0CU, 'S', 'T', '$', 'P', 'o', 's', '$', 's', 't', 'V', 'a', 'l',
        0xA0U, 0x03U, 0x83U, 0x01U, 0xFFU
    };
    const uint8_t expected_service[] = {
        0x30U, 0x22U,
        0xA0U, 0x1BU,
        0x30U, 0x19U,
        0xA0U, 0x17U,
        0xA1U, 0x15U,
        0x1AU, 0x05U, 'X', 'C', 'B', 'R', '1',
        0x1AU, 0x0CU, 'S', 'T', '$', 'P', 'o', 's', '$', 's', 't', 'V', 'a', 'l',
        0xA0U, 0x03U, 0x83U, 0x01U, 0xFFU
    };
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    size_t invoke_consumed_length = 0U;
    size_t service_consumed_length = 0U;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_pdu_init(&decoded_pdu);
    unitlab_mms_association_frame_init(&association_frame);

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &boolean_value;
    data_element.value_length = 1U;

    assert(unitlab_mms_build_write_request_frame("XCBR1", "ST$Pos$stVal", &data_element, 9U, scratch, sizeof(scratch), frame, sizeof(frame), &encoded_length, &diagnostic) == 1);
    assert(encoded_length > 0U);
    assert(unitlab_mms_association_frame_decode(&association_frame, frame, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);

    assert(unitlab_mms_pdu_decode(&decoded_pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed_length, &diagnostic) == 1);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
    assert(decoded_pdu.pdu_length == sizeof(expected_payload));
    assert(memcmp(decoded_pdu.pdu_bytes, expected_payload, sizeof(expected_payload)) == 0);

    unitlab_mms_ber_element_init(&invoke_id_element);
    assert(unitlab_mms_ber_read(&invoke_id_element, decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, &invoke_consumed_length, &diagnostic) == 1);
    assert(invoke_id_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL);
    assert(invoke_id_element.tag.tag_number == 2U);
    assert(invoke_id_element.value_length == 1U);
    assert(invoke_id_element.value_bytes[0] == 9U);

    unitlab_mms_ber_element_init(&service_element);
    assert(unitlab_mms_ber_read(&service_element, decoded_pdu.pdu_bytes + invoke_consumed_length, decoded_pdu.pdu_length - invoke_consumed_length, &service_consumed_length, &diagnostic) == 1);
    assert(service_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(service_element.tag.tag_number == 5U);
    assert(service_element.tag.constructed == 1);
    assert(service_element.value_length == sizeof(expected_service));
    assert(memcmp(service_element.value_bytes, expected_service, sizeof(expected_service)) == 0);

    unitlab_mms_pdu_init(&decoded_pdu);
    decoded_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    decoded_pdu.has_invoke_id = 1;
    decoded_pdu.invoke_id = 9U;
    decoded_pdu.has_service = 1;
    decoded_pdu.service_kind = UNITLAB_MMS_SERVICE_WRITE;
    decoded_pdu.service_bytes = service_element.value_bytes;
    decoded_pdu.service_length = service_element.value_length;
    assert(unitlab_mms_semantic_result_from_wire_pdu(&semantic_result, &decoded_pdu, &bridge_diagnostic) == 1);
    assert(semantic_result.ok == 1);
    assert(semantic_result.pdu.kind == UNITLAB_MMS_DECODED_PDU_WRITE_REQUEST);
    assert(strcmp(semantic_result.pdu.domain_id, "XCBR1") == 0);
    assert(strcmp(semantic_result.pdu.item_id, "ST$Pos$stVal") == 0);
    assert(strcmp(semantic_result.pdu.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(strcmp(semantic_result.pdu.attribute_reference, "stVal") == 0);
    assert(semantic_result.pdu.value_length == 1U);
    assert(semantic_result.pdu.value_bytes[0] == 0xFFU);
    assert(semantic_result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_mms_get_variable_access_attributes_request_wire_frame_builder_roundtrip(void)
{
    uint8_t scratch[256];
    uint8_t frame[256];
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsPdu decoded_pdu;
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsDecodeDiagnostic bridge_diagnostic;
    UnitLabMmsDiagnostic diagnostic;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    size_t invoke_consumed_length = 0U;
    size_t service_consumed_length = 0U;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_pdu_init(&decoded_pdu);
    unitlab_mms_association_frame_init(&association_frame);

    assert(unitlab_mms_build_get_variable_access_attributes_request_frame("XCBR1", "ST$Pos$stVal", 7U, scratch, sizeof(scratch), frame, sizeof(frame), &encoded_length, &diagnostic) == 1);
    assert(encoded_length > 0U);
    assert(unitlab_mms_association_frame_decode(&association_frame, frame, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);

    assert(unitlab_mms_pdu_decode(&decoded_pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed_length, &diagnostic) == 1);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
    assert(decoded_pdu.pdu_length > 0U);

    unitlab_mms_ber_element_init(&invoke_id_element);
    assert(unitlab_mms_ber_read(&invoke_id_element, decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, &invoke_consumed_length, &diagnostic) == 1);
    assert(invoke_id_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL);
    assert(invoke_id_element.tag.tag_number == 2U);
    assert(invoke_id_element.value_length == 1U);
    assert(invoke_id_element.value_bytes[0] == 7U);

    unitlab_mms_ber_element_init(&service_element);
    assert(unitlab_mms_ber_read(&service_element, decoded_pdu.pdu_bytes + invoke_consumed_length, decoded_pdu.pdu_length - invoke_consumed_length, &service_consumed_length, &diagnostic) == 1);
    assert(service_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(service_element.tag.tag_number == 6U);
    assert(service_element.tag.constructed == 1);

    unitlab_mms_pdu_init(&decoded_pdu);
    decoded_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    decoded_pdu.has_invoke_id = 1;
    decoded_pdu.invoke_id = 7U;
    decoded_pdu.has_service = 1;
    decoded_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES;
    decoded_pdu.service_bytes = service_element.value_bytes;
    decoded_pdu.service_length = service_element.value_length;
    assert(unitlab_mms_semantic_result_from_wire_pdu(&semantic_result, &decoded_pdu, &bridge_diagnostic) == 1);
    assert(semantic_result.ok == 1);
    assert(semantic_result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(semantic_result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_REQUEST);
    assert(strcmp(semantic_result.pdu.domain_id, "XCBR1") == 0);
    assert(strcmp(semantic_result.pdu.item_id, "ST$Pos$stVal") == 0);
    assert(strcmp(semantic_result.pdu.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(strcmp(semantic_result.pdu.attribute_reference, "stVal") == 0);
    assert(semantic_result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_mms_get_name_list_request_wire_frame_builder_roundtrip(void)
{
    uint8_t scratch[256];
    uint8_t frame[256];
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsPdu decoded_pdu;
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsDecodeDiagnostic bridge_diagnostic;
    UnitLabMmsDiagnostic diagnostic;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    size_t invoke_consumed_length = 0U;
    size_t service_consumed_length = 0U;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_pdu_init(&decoded_pdu);
    unitlab_mms_association_frame_init(&association_frame);

    assert(unitlab_mms_build_get_name_list_request_frame(2U, 1U, "LD0", NULL, 61U, scratch, sizeof(scratch), frame, sizeof(frame), &encoded_length, &diagnostic) == 1);
    assert(encoded_length > 0U);
    assert(unitlab_mms_association_frame_decode(&association_frame, frame, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);

    assert(unitlab_mms_pdu_decode(&decoded_pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed_length, &diagnostic) == 1);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);

    unitlab_mms_ber_element_init(&invoke_id_element);
    assert(unitlab_mms_ber_read(&invoke_id_element, decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, &invoke_consumed_length, &diagnostic) == 1);
    assert(invoke_id_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL);
    assert(invoke_id_element.tag.tag_number == 2U);
    assert(invoke_id_element.value_length == 1U);
    assert(invoke_id_element.value_bytes[0] == 61U);

    unitlab_mms_ber_element_init(&service_element);
    assert(unitlab_mms_ber_read(&service_element, decoded_pdu.pdu_bytes + invoke_consumed_length, decoded_pdu.pdu_length - invoke_consumed_length, &service_consumed_length, &diagnostic) == 1);
    assert(service_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(service_element.tag.tag_number == 1U);
    assert(service_element.tag.constructed == 1);

    unitlab_mms_pdu_init(&decoded_pdu);
    decoded_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    decoded_pdu.has_invoke_id = 1;
    decoded_pdu.invoke_id = 61U;
    decoded_pdu.has_service = 1;
    decoded_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAME_LIST;
    decoded_pdu.service_bytes = service_element.value_bytes;
    decoded_pdu.service_length = service_element.value_length;
    assert(unitlab_mms_semantic_result_from_wire_pdu(&semantic_result, &decoded_pdu, &bridge_diagnostic) == 1);
    assert(semantic_result.ok == 1);
    assert(semantic_result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_REQUEST);
    assert(semantic_result.pdu.object_class == 2U);
    assert(semantic_result.pdu.object_scope == 1U);
    assert(strcmp(semantic_result.pdu.domain_id, "LD0") == 0);
}

static void test_mms_pdu_conclude_roundtrip(void)
{
    struct {
        UnitLabMmsPduKind kind;
        uint8_t tag;
    } cases[] = {
        { UNITLAB_MMS_PDU_CONCLUDE_REQUEST, 0x6BU },
        { UNITLAB_MMS_PDU_CONCLUDE_RESPONSE, 0x6CU },
        { UNITLAB_MMS_PDU_CONCLUDE_ERROR, 0x6DU },
    };
    uint8_t buffer[32];
    UnitLabMmsPdu pdu;
    UnitLabMmsPdu decoded_pdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[4] = { 0x80U, 0x01U, 0x00U, 0x00U };

    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        unitlab_mms_diagnostic_clear(&diagnostic);
        unitlab_mms_pdu_init(&pdu);
        pdu.kind = cases[i].kind;
        pdu.pdu_bytes = payload;
        pdu.pdu_length = sizeof(payload);
        assert(unitlab_mms_pdu_encode(&pdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
        assert(buffer[0] == cases[i].tag);
        unitlab_mms_pdu_init(&decoded_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
        assert(consumed_length == encoded_length);
        assert(decoded_pdu.kind == cases[i].kind);
        assert(decoded_pdu.pdu_length > 0U);
        }
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
    struct {
        size_t value_length;
        size_t expected_encoded_length;
        uint8_t expected_bytes[3];
        size_t expected_bytes_length;
    } cases[] = {
        { 0U, 1U, { 0x00U, 0x00U, 0x00U }, 1U },
        { 1U, 1U, { 0x01U, 0x00U, 0x00U }, 1U },
        { 2U, 1U, { 0x02U, 0x00U, 0x00U }, 1U },
        { 10U, 1U, { 0x0AU, 0x00U, 0x00U }, 1U },
        { 127U, 1U, { 0x7FU, 0x00U, 0x00U }, 1U },
        { 128U, 2U, { 0x81U, 0x80U, 0x00U }, 2U },
        { 255U, 2U, { 0x81U, 0xFFU, 0x00U }, 2U },
        { 256U, 3U, { 0x82U, 0x01U, 0x00U }, 3U },
        { 65535U, 3U, { 0x82U, 0xFFU, 0xFFU }, 3U },
    };
    uint8_t buffer[16];
    size_t encoded_length = 0U;
    size_t decoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t expected_bytes_length = cases[i].expected_bytes_length;

        assert(unitlab_mms_ber_length_encode(cases[i].value_length, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
        assert(encoded_length == cases[i].expected_encoded_length);
        assert(memcmp(buffer, cases[i].expected_bytes, expected_bytes_length) == 0);
        assert(unitlab_mms_ber_length_decode(&decoded_length, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
        assert(decoded_length == cases[i].value_length);
        assert(consumed_length == encoded_length);
    }

    {
        const uint8_t empty_length[1] = { 0x00U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, empty_length, 0U, &consumed_length, &diagnostic) == 0);
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL);
        assert(diagnostic.message[0] != '\0');
    }

    {
        const uint8_t indefinite_length[1] = { 0x80U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, indefinite_length, sizeof(indefinite_length), &consumed_length, &diagnostic) == 0);
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
        assert(diagnostic.message[0] != '\0');
    }

    {
        const uint8_t leading_zero_length[3] = { 0x82U, 0x00U, 0x80U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, leading_zero_length, sizeof(leading_zero_length), &consumed_length, &diagnostic) == 0);
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
        assert(diagnostic.message[0] != '\0');
    }

    {
        const uint8_t truncated_length[2] = { 0x82U, 0x01U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, truncated_length, sizeof(truncated_length), &consumed_length, &diagnostic) == 0);
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL);
        assert(diagnostic.message[0] != '\0');
    }

    {
        const uint8_t unsupported_octet_count[10] = { 0x89U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U };
        assert(unitlab_mms_ber_length_decode(&decoded_length, unsupported_octet_count, sizeof(unsupported_octet_count), &consumed_length, &diagnostic) == 0);
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
        assert(diagnostic.message[0] != '\0');
    }
}

static void test_ber_tag_roundtrip(void)
{
    struct {
        UnitLabMmsBerTag tag;
        uint8_t expected_bytes[2];
        size_t expected_length;
    } cases[] = {
        { { UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U }, { 0x02U, 0x00U }, 1U },
        { { UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 1, 16U }, { 0x30U, 0x00U }, 1U },
        { { UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 0U }, { 0xA0U, 0x00U }, 1U },
        { { UNITLAB_MMS_BER_TAG_CLASS_APPLICATION, 0, 9U }, { 0x49U, 0x00U }, 1U },
        { { UNITLAB_MMS_BER_TAG_CLASS_PRIVATE, 0, 1U }, { 0xC1U, 0x00U }, 1U },
    };
    uint8_t buffer[16];
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsBerTag decoded_tag;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        unitlab_mms_ber_tag_init(&decoded_tag);
        assert(unitlab_mms_ber_tag_encode(&cases[i].tag, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
        assert(encoded_length == cases[i].expected_length);
        assert(memcmp(buffer, cases[i].expected_bytes, encoded_length) == 0);
        assert(unitlab_mms_ber_tag_decode(&decoded_tag, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
        assert(consumed_length == encoded_length);
        assert(decoded_tag.tag_class == cases[i].tag.tag_class);
        assert(decoded_tag.constructed == cases[i].tag.constructed);
        assert(decoded_tag.tag_number == cases[i].tag.tag_number);
    }
}

static void test_ber_tag_long_form_valid(void)
{
    uint8_t buffer[16];
    UnitLabMmsBerTag tag;
    UnitLabMmsBerTag decoded_tag;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);

    unitlab_mms_ber_tag_init(&tag);
    tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    tag.constructed = 0;
    tag.tag_number = 31U;
    assert(unitlab_mms_ber_tag_encode(&tag, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == 2U);
    assert(memcmp(buffer, (const uint8_t[]){ 0x9FU, 0x1FU }, encoded_length) == 0);
    unitlab_mms_ber_tag_init(&decoded_tag);
    assert(unitlab_mms_ber_tag_decode(&decoded_tag, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_tag.constructed == 0);
    assert(decoded_tag.tag_number == 31U);

    unitlab_mms_ber_tag_init(&tag);
    tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    tag.constructed = 0;
    tag.tag_number = 128U;
    assert(unitlab_mms_ber_tag_encode(&tag, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(encoded_length == 3U);
    assert(memcmp(buffer, (const uint8_t[]){ 0x9FU, 0x81U, 0x00U }, encoded_length) == 0);
    unitlab_mms_ber_tag_init(&decoded_tag);
    assert(unitlab_mms_ber_tag_decode(&decoded_tag, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_tag.constructed == 0);
    assert(decoded_tag.tag_number == 128U);
}

static void test_ber_tag_rejects_non_minimal_long_form(void)
{
    const uint8_t tag_bytes[3] = { 0x9FU, 0x80U, 0x1FU };
    UnitLabMmsBerTag decoded_tag;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_tag_init(&decoded_tag);
    assert(unitlab_mms_ber_tag_decode(&decoded_tag, tag_bytes, sizeof(tag_bytes), &consumed_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
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
    assert(diagnostic.message[0] != '\0');
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
    assert(diagnostic.message[0] != '\0');
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
    assert(decoded_element.encoded_length == encoded_length);
    assert(decoded_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_APPLICATION);
    assert(decoded_element.tag.tag_number == 2U);
    assert(decoded_element.value_length == sizeof(value));
    assert(memcmp(decoded_element.value_bytes, value, sizeof(value)) == 0);
}

static void test_ber_element_trailing_bytes(void)
{
    const uint8_t buffer[5] = { 0x02U, 0x01U, 0x2AU, 0xDEU, 0xADU };
    UnitLabMmsBerElement decoded_element;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&decoded_element);
    assert(unitlab_mms_ber_read(&decoded_element, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 1);
    assert(consumed_length == 3U);
    assert(decoded_element.encoded_length == consumed_length);
    assert(consumed_length < sizeof(buffer));
    assert(memcmp(&buffer[consumed_length], (const uint8_t[]){ 0xDEU, 0xADU }, sizeof(buffer) - consumed_length) == 0);
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
    assert(diagnostic.message[0] != '\0');
}

int main(void)
{
    test_tpkt_write_header();
    test_tpkt_minimal_roundtrip();
    test_tpkt_roundtrip();
    test_tpkt_unwrap_ignores_trailing_bytes();
    test_tpkt_rejects_invalid_version();
    test_tpkt_rejects_length_less_than_header();
    test_tpkt_rejects_declared_length_larger_than_buffer();
    test_cotp_cr_golden();
    test_cotp_cc_golden();
    test_cotp_rejects_malformed_inputs();
    test_cotp_encode_rejects_missing_user_data();
    test_cotp_cr_roundtrip();
    test_cotp_connect_request_frame_smoke();
    test_cotp_connect_response_frame_smoke();
    test_cotp_dt_roundtrip();
    test_cotp_decode_stops_at_indicated_length();
    test_transport_frame_roundtrip_cr_cc_dt();
    test_transport_frame_rejects_trailing_bytes();
    test_transport_frame_roundtrip();
    test_wire_frame_builder_information_report_roundtrip();
    test_association_frame_decode_roundtrip();
    test_association_frame_encode_roundtrip();
    test_association_frame_fully_encoded_roundtrip();
    test_wire_frame_builder_aarq_association_roundtrip();
    // test_association_response_frame_roundtrip();
    test_association_response_frame_smoke();
    test_mms_pdu_initiate_roundtrip();
    test_acse_association_accept_frame_roundtrip();
    test_acse_top_level_roundtrips();
    test_acse_decode_stops_at_indicated_length();
    test_acse_raw_field_view();
    test_session_spdu_roundtrip_long_payload();
    test_session_spdu_rejects_truncated_data_transfer();
    test_session_spdu_rejects_invalid_data_transfer_header();
    test_session_spdu_rejects_unsupported_kind();
    test_session_spdu_decode_stops_at_indicated_length();
    test_session_spdu_decode_reports_consumed_length_with_trailing_bytes();
    test_session_spdu_decode_resets_output_on_failure();
    test_session_spdu_roundtrips();
    test_session_spdu_rejects_mismatched_declared_kind_and_code();
    test_presentation_simply_encoded_roundtrip();
    test_presentation_simply_encoded_zero_length_payload();
    test_presentation_fully_encoded_roundtrip();
    test_acse_decode_accepts_raw_aarq_fields();
    test_presentation_decode_accepts_pdv_list_wrapper();
    test_presentation_decode_rejects_malformed_pdv_list_shape();
    test_presentation_decode_reports_trailing_bytes();
    test_presentation_decode_resets_output_on_failure();
    test_presentation_decode_rejects_unsupported_outer_tag();
    test_presentation_encode_rejects_non_supported_kind();
    test_presentation_encode_rejects_null_payload_bytes();
    test_presentation_rejects_truncated_ber();
    test_mms_pdu_confirmed_request_roundtrip();
    test_mms_pdu_confirmed_response_roundtrip();
    test_initiate_response_profile_uses_model_plan();
    test_initiate_response_profile_scales_with_larger_model();
    test_mms_pdu_decode_stops_at_indicated_length();
    test_mms_pdu_unconfirmed_roundtrip();
    test_mms_pdu_confirmed_request_roundtrip_with_wide_invoke_id();
    test_mms_confirmed_request_roundtrip_with_allocated_invoke_id();
    test_mms_read_request_wire_frame_builder_roundtrip();
    test_mms_write_request_wire_frame_builder_roundtrip();
    test_mms_get_variable_access_attributes_request_wire_frame_builder_roundtrip();
    test_mms_get_name_list_request_wire_frame_builder_roundtrip();
    test_mms_pdu_conclude_roundtrip();
    test_mms_pdu_rejects_non_minimal_invoke_id();
    test_ber_length_roundtrip();
    test_ber_tag_roundtrip();
    test_ber_tag_long_form_valid();
    test_ber_tag_rejects_non_minimal_long_form();
    test_ber_tag_rejects_truncated_long_form();
    test_ber_tag_rejects_overflow_long_form();
    test_ber_element_roundtrip();
    test_ber_element_trailing_bytes();
    test_ber_element_rejects_missing_value_bytes();
    return 0;
}
