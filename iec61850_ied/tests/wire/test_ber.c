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
