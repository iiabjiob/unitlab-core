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
int main(void)
{
    test_session_spdu_roundtrip_long_payload();
    test_session_spdu_decode_stops_at_indicated_length();
    test_session_spdu_rejects_truncated_data_transfer();
    test_session_spdu_rejects_invalid_data_transfer_header();
    test_session_spdu_rejects_unsupported_kind();
    test_session_spdu_decode_reports_consumed_length_with_trailing_bytes();
    test_session_spdu_decode_resets_output_on_failure();
    test_session_spdu_roundtrips();
    test_session_spdu_rejects_mismatched_declared_kind_and_code();
    return 0;
}
