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
    assert(buffer[0] == 0xA0U);
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
    assert(buffer[0] == 0xA1U);
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
static void test_mms_pdu_supported_other_roundtrips(void)
{
    struct {
        UnitLabMmsPduKind kind;
        uint8_t tag;
        const uint8_t payload[6];
        size_t payload_length;
    } cases[] = {
        { UNITLAB_MMS_PDU_CONFIRMED_ERROR, 0xA2U, { 0x02U, 0x01U, 0x07U, 0xA0U, 0x01U, 0x00U }, 6U },
        { UNITLAB_MMS_PDU_REJECT, 0xA4U, { 0x80U, 0x01U, 0x01U, 0x00U, 0x00U, 0x00U }, 3U },
    };
    uint8_t buffer[32];
    UnitLabMmsPdu pdu;
    UnitLabMmsPdu decoded_pdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        unitlab_mms_diagnostic_clear(&diagnostic);
        unitlab_mms_pdu_init(&pdu);
        pdu.kind = cases[i].kind;
        pdu.pdu_bytes = cases[i].payload;
        pdu.pdu_length = cases[i].payload_length;
        assert(unitlab_mms_pdu_encode(&pdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
        assert(buffer[0] == cases[i].tag);
        unitlab_mms_pdu_init(&decoded_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
        assert(consumed_length == encoded_length);
        assert(decoded_pdu.kind == cases[i].kind);
        assert(decoded_pdu.pdu_length == cases[i].payload_length);
        assert(decoded_pdu.pdu_bytes == &buffer[2]);
        assert(memcmp(decoded_pdu.pdu_bytes, cases[i].payload, cases[i].payload_length) == 0);
    }
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
    assert(decoded_pdu.pdu_bytes == &buffer[2]);
    assert(decoded_pdu.has_invoke_id == 1);
    assert(decoded_pdu.invoke_id == 5U);
    assert(decoded_pdu.has_service == 1);
    assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
    assert(decoded_pdu.service_length == 1U);
    assert(decoded_pdu.service_bytes[0] == 0xAAU);
}
static void test_mms_pdu_decode_rejects_unsupported_top_level_tag(void)
{
    uint8_t buffer[16];
    UnitLabMmsBerElement element;
    UnitLabMmsPdu decoded_pdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[1] = { 0x00U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
    element.tag.constructed = 1;
    element.tag.tag_number = 99U;
    element.value_bytes = payload;
    element.value_length = sizeof(payload);
    assert(unitlab_mms_ber_write(&element, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);

    unitlab_mms_pdu_init(&decoded_pdu);
    decoded_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    decoded_pdu.pdu_bytes = (const uint8_t*)0x1;
    decoded_pdu.pdu_length = 11U;
    decoded_pdu.has_invoke_id = 1;
    decoded_pdu.has_service = 1;
    decoded_pdu.service_bytes = (const uint8_t*)0x2;
    decoded_pdu.service_length = 22U;
    decoded_pdu.encoded_length = 33U;
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, encoded_length, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_NONE);
    assert(decoded_pdu.pdu_bytes == NULL);
    assert(decoded_pdu.pdu_length == 0U);
    assert(decoded_pdu.has_invoke_id == 0);
    assert(decoded_pdu.has_service == 0);
    assert(decoded_pdu.service_bytes == NULL);
    assert(decoded_pdu.service_length == 0U);
    assert(decoded_pdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
    assert(diagnostic.message[0] != '\0');
}
static void test_mms_pdu_encode_rejects_null_pdu_bytes(void)
{
    uint8_t buffer[16];
    UnitLabMmsPdu pdu;
    size_t encoded_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    pdu.pdu_bytes = NULL;
    pdu.pdu_length = 1U;
    assert(unitlab_mms_pdu_encode(&pdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 0);
    assert(encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(diagnostic.message[0] != '\0');
}
static void test_mms_pdu_decode_rejects_missing_invoke_id(void)
{
    const uint8_t buffer[2] = { 0xA0U, 0x00U };
    UnitLabMmsPdu decoded_pdu;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&decoded_pdu);
    decoded_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    decoded_pdu.pdu_bytes = (const uint8_t*)0x1;
    decoded_pdu.pdu_length = 11U;
    decoded_pdu.has_invoke_id = 1;
    decoded_pdu.has_service = 1;
    decoded_pdu.service_bytes = (const uint8_t*)0x2;
    decoded_pdu.service_length = 22U;
    decoded_pdu.encoded_length = 33U;
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_NONE);
    assert(decoded_pdu.pdu_bytes == NULL);
    assert(decoded_pdu.pdu_length == 0U);
    assert(decoded_pdu.has_invoke_id == 0);
    assert(decoded_pdu.has_service == 0);
    assert(decoded_pdu.service_bytes == NULL);
    assert(decoded_pdu.service_length == 0U);
    assert(decoded_pdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
}
static void test_mms_pdu_decode_rejects_wrong_invoke_id_tag(void)
{
    const uint8_t buffer[8] = { 0xA0U, 0x06U, 0xA0U, 0x03U, 0x80U, 0x01U, 0x00U, 0xA4U };
    UnitLabMmsPdu decoded_pdu;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&decoded_pdu);
    decoded_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    decoded_pdu.pdu_bytes = (const uint8_t*)0x1;
    decoded_pdu.pdu_length = 77U;
    decoded_pdu.has_invoke_id = 1;
    decoded_pdu.has_service = 1;
    decoded_pdu.service_bytes = (const uint8_t*)0x2;
    decoded_pdu.service_length = 88U;
    decoded_pdu.encoded_length = 99U;
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_NONE);
    assert(decoded_pdu.pdu_bytes == NULL);
    assert(decoded_pdu.pdu_length == 0U);
    assert(decoded_pdu.has_invoke_id == 0);
    assert(decoded_pdu.has_service == 0);
    assert(decoded_pdu.service_bytes == NULL);
    assert(decoded_pdu.service_length == 0U);
    assert(decoded_pdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
}
static void test_mms_pdu_decode_rejects_non_minimal_invoke_id(void)
{
    const uint8_t buffer[9] = { 0xA0U, 0x07U, 0x02U, 0x02U, 0x00U, 0x01U, 0xA4U, 0x01U, 0xAAU };
    UnitLabMmsPdu decoded_pdu;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&decoded_pdu);
    decoded_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    decoded_pdu.pdu_bytes = (const uint8_t*)0x1;
    decoded_pdu.pdu_length = 77U;
    decoded_pdu.has_invoke_id = 1;
    decoded_pdu.has_service = 1;
    decoded_pdu.service_bytes = (const uint8_t*)0x2;
    decoded_pdu.service_length = 88U;
    decoded_pdu.encoded_length = 99U;
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_NONE);
    assert(decoded_pdu.pdu_bytes == NULL);
    assert(decoded_pdu.pdu_length == 0U);
    assert(decoded_pdu.has_invoke_id == 0);
    assert(decoded_pdu.has_service == 0);
    assert(decoded_pdu.service_bytes == NULL);
    assert(decoded_pdu.service_length == 0U);
    assert(decoded_pdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
}
static void test_mms_pdu_decode_rejects_invoke_id_overflow(void)
{
    const uint8_t buffer[9] = { 0xA0U, 0x07U, 0x02U, 0x05U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U };
    UnitLabMmsPdu decoded_pdu;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&decoded_pdu);
    decoded_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    decoded_pdu.pdu_bytes = (const uint8_t*)0x1;
    decoded_pdu.pdu_length = 77U;
    decoded_pdu.has_invoke_id = 1;
    decoded_pdu.has_service = 1;
    decoded_pdu.service_bytes = (const uint8_t*)0x2;
    decoded_pdu.service_length = 88U;
    decoded_pdu.encoded_length = 99U;
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_NONE);
    assert(decoded_pdu.pdu_bytes == NULL);
    assert(decoded_pdu.pdu_length == 0U);
    assert(decoded_pdu.has_invoke_id == 0);
    assert(decoded_pdu.has_service == 0);
    assert(decoded_pdu.service_bytes == NULL);
    assert(decoded_pdu.service_length == 0U);
    assert(decoded_pdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
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
    assert(buffer[0] == 0xA3U);
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
    assert(buffer[0] == 0xA0U);
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
static void test_mms_pdu_conclude_roundtrip(void)
{
    struct {
        UnitLabMmsPduKind kind;
        uint8_t tag;
    } cases[] = {
        { UNITLAB_MMS_PDU_CONCLUDE_REQUEST, 0x8BU },
        { UNITLAB_MMS_PDU_CONCLUDE_RESPONSE, 0x8CU },
        { UNITLAB_MMS_PDU_CONCLUDE_ERROR, 0x8DU },
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
    const uint8_t buffer[6] = { 0xA0U, 0x04U, 0x02U, 0x02U, 0x00U, 0x01U };
    UnitLabMmsPdu decoded_pdu;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}
int main(void)
{
    test_mms_pdu_confirmed_request_roundtrip();
    test_mms_pdu_confirmed_response_roundtrip();
    test_mms_pdu_supported_other_roundtrips();
    test_mms_pdu_decode_stops_at_indicated_length();
    test_mms_pdu_decode_rejects_unsupported_top_level_tag();
    test_mms_pdu_encode_rejects_null_pdu_bytes();
    test_mms_pdu_decode_rejects_missing_invoke_id();
    test_mms_pdu_decode_rejects_wrong_invoke_id_tag();
    test_mms_pdu_decode_rejects_non_minimal_invoke_id();
    test_mms_pdu_decode_rejects_invoke_id_overflow();
    test_mms_pdu_unconfirmed_roundtrip();
    test_mms_pdu_confirmed_request_roundtrip_with_wide_invoke_id();
    test_mms_pdu_conclude_roundtrip();
    test_mms_pdu_rejects_non_minimal_invoke_id();
    return 0;
}
