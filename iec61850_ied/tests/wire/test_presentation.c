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
int main(void)
{
    test_presentation_simply_encoded_roundtrip();
    test_presentation_simply_encoded_zero_length_payload();
    test_presentation_decode_rejects_malformed_pdv_list_shape();
    test_presentation_decode_reports_trailing_bytes();
    test_presentation_decode_resets_output_on_failure();
    test_presentation_fully_encoded_roundtrip();
    test_presentation_decode_accepts_pdv_list_wrapper();
    test_presentation_decode_rejects_unsupported_outer_tag();
    test_presentation_encode_rejects_non_supported_kind();
    test_presentation_encode_rejects_null_payload_bytes();
    test_presentation_rejects_truncated_ber();
    return 0;
}
