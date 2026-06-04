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
static void test_acse_decode_rejects_unsupported_top_level_tag(void)
{
    uint8_t buffer[32];
    UnitLabMmsAcseApdu apdu;
    UnitLabMmsBerElement element;
    size_t encoded_length = 0U;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[2] = { 0x80U, 0x00U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
    element.tag.constructed = 1;
    element.tag.tag_number = 99U;
    element.value_bytes = payload;
    element.value_length = sizeof(payload);
    assert(unitlab_mms_ber_write(&element, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);

    unitlab_mms_acse_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_ACSE_APDU_AARE;
    apdu.apdu_bytes = (const uint8_t*)0x1;
    apdu.apdu_length = 11U;
    apdu.field_count = 7U;
    apdu.encoded_length = 22U;
    assert(unitlab_mms_acse_decode(&apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(apdu.kind == UNITLAB_MMS_ACSE_APDU_NONE);
    assert(apdu.apdu_bytes == NULL);
    assert(apdu.apdu_length == 0U);
    assert(apdu.field_count == 0U);
    assert(apdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
    assert(diagnostic.message[0] != '\0');
}
static void test_acse_decode_rejects_malformed_raw_field_list(void)
{
    const uint8_t buffer[] = {
        0xA0U, 0x03U, 0x80U, 0x01U, 0x00U,
        0xA2U, 0x03U, 0x81U, 0x01U,
    };
    UnitLabMmsAcseApdu apdu;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_acse_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    apdu.apdu_bytes = (const uint8_t*)0x1;
    apdu.apdu_length = 9U;
    apdu.field_count = 5U;
    apdu.encoded_length = 10U;
    assert(unitlab_mms_acse_decode(&apdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(apdu.kind == UNITLAB_MMS_ACSE_APDU_NONE);
    assert(apdu.apdu_bytes == NULL);
    assert(apdu.apdu_length == 0U);
    assert(apdu.field_count == 0U);
    assert(apdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL || diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(diagnostic.message[0] != '\0');
}
static void test_acse_decode_rejects_too_many_fields(void)
{
    uint8_t buffer[50];
    UnitLabMmsAcseApdu apdu;
    size_t consumed_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    buffer[0] = 0xA0U;
    buffer[1] = 0x00U;
    for (size_t i = 0U; i < 16U; i++) {
        size_t offset = 2U + (i * 3U);
        buffer[offset + 0U] = 0x80U;
        buffer[offset + 1U] = 0x01U;
        buffer[offset + 2U] = (uint8_t)i;
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_acse_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_ACSE_APDU_AARE;
    apdu.apdu_bytes = (const uint8_t*)0x1;
    apdu.apdu_length = 11U;
    apdu.field_count = 8U;
    apdu.encoded_length = 13U;
    assert(unitlab_mms_acse_decode(&apdu, buffer, sizeof(buffer), &consumed_length, &diagnostic) == 0);
    assert(consumed_length == 0U);
    assert(apdu.kind == UNITLAB_MMS_ACSE_APDU_NONE);
    assert(apdu.apdu_bytes == NULL);
    assert(apdu.apdu_length == 0U);
    assert(apdu.field_count == 0U);
    assert(apdu.encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
    assert(diagnostic.message[0] != '\0');
}
static void test_acse_encode_rejects_null_apdu_bytes(void)
{
    uint8_t buffer[16];
    UnitLabMmsAcseApdu apdu;
    size_t encoded_length = 123U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_acse_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    apdu.apdu_bytes = NULL;
    apdu.apdu_length = 1U;
    assert(unitlab_mms_acse_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 0);
    assert(encoded_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT);
    assert(diagnostic.message[0] != '\0');
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
    const uint8_t payload[8] = { 0x80U, 0x01U, 0x00U, 0x81U, 0x01U, 0x2AU, 0x82U, 0x00U };

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
int main(void)
{
    test_acse_decode_accepts_raw_aarq_fields();
    test_acse_decode_rejects_unsupported_top_level_tag();
    test_acse_decode_rejects_malformed_raw_field_list();
    test_acse_decode_rejects_too_many_fields();
    test_acse_encode_rejects_null_apdu_bytes();
    test_acse_decode_stops_at_indicated_length();
    test_acse_raw_field_view();
    test_acse_top_level_roundtrips();
    test_acse_association_accept_frame_roundtrip();
    return 0;
}
