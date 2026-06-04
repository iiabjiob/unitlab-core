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
int main(void)
{
    test_transport_frame_roundtrip_cr_cc_dt();
    test_transport_frame_rejects_trailing_bytes();
    test_cotp_connect_request_frame_smoke();
    test_cotp_connect_response_frame_smoke();
    test_transport_frame_roundtrip();
    return 0;
}
