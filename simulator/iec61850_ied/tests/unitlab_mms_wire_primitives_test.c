#include "../src/wire/acse/unitlab_mms_acse.h"
#include "../src/wire/ber/unitlab_mms_ber.h"
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
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_wrap(payload, sizeof(payload), frame, sizeof(frame), &frame_length, &diagnostic) == 1);
    assert(frame_length == 8U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(unitlab_mms_tpkt_unwrap(frame, frame_length, &decoded_payload, &payload_length, &diagnostic) == 1);
    assert(payload_length == sizeof(payload));
    assert(memcmp(decoded_payload, payload, sizeof(payload)) == 0);
}

static void test_tpkt_rejects_invalid_version(void)
{
    const uint8_t frame[4] = { 2U, 0U, 0U, 4U };
    const uint8_t* decoded_payload = NULL;
    size_t payload_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_tpkt_unwrap(frame, sizeof(frame), &decoded_payload, &payload_length, &diagnostic) == 0);
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
    assert(decoded_tpdu.user_data_length == sizeof(user_data));
    assert(memcmp(decoded_tpdu.user_data, user_data, sizeof(user_data)) == 0);
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
    assert(decoded_tpdu.user_data_length == sizeof(user_data));
    assert(memcmp(decoded_tpdu.user_data, user_data, sizeof(user_data)) == 0);
}

static void test_acse_aarq_roundtrip(void)
{
    uint8_t buffer[32];
    UnitLabMmsAcseApdu apdu;
    UnitLabMmsAcseApdu decoded_apdu;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[4] = { 0x30U, 0x02U, 0x01U, 0x01U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_acse_apdu_init(&apdu);
    apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    apdu.apdu_bytes = payload;
    apdu.apdu_length = sizeof(payload);
    assert(unitlab_mms_acse_encode(&apdu, buffer, sizeof(buffer), &encoded_length, &diagnostic) == 1);
    assert(buffer[0] == 0x60U);
    unitlab_mms_acse_apdu_init(&decoded_apdu);
    assert(unitlab_mms_acse_decode(&decoded_apdu, buffer, encoded_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == encoded_length);
    assert(decoded_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARQ);
    assert(decoded_apdu.apdu_length == sizeof(payload));
    assert(memcmp(decoded_apdu.apdu_bytes, payload, sizeof(payload)) == 0);
}

static void test_ber_length_roundtrip(void)
{
    uint8_t buffer[8];
    size_t encoded_length = 0U;
    size_t decoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_diagnostic_clear(&diagnostic);
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
}

static void test_ber_tag_roundtrip(void)
{
    uint8_t buffer[8];
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

int main(void)
{
    test_tpkt_roundtrip();
    test_tpkt_rejects_invalid_version();
    test_cotp_cr_roundtrip();
    test_cotp_dt_roundtrip();
    test_acse_aarq_roundtrip();
    test_ber_length_roundtrip();
    test_ber_tag_roundtrip();
    test_ber_element_roundtrip();
    return 0;
}
