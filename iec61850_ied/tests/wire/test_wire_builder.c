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
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(decoded_fixture.presentation.payload_length > 0U);
    assert(decoded_fixture.presentation.payload_bytes[0] == 0xA3U);

    unitlab_mms_pdu_init(&decoded_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_pdu, decoded_fixture.presentation.payload_bytes, decoded_fixture.presentation.payload_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == decoded_fixture.presentation.payload_length);
    assert(decoded_pdu.kind == UNITLAB_MMS_PDU_UNCONFIRMED);
    assert(decoded_pdu.has_service == 1);
    assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_INFORMATION_REPORT);
    assert(decoded_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(decoded_pdu.service_tag.constructed == 1);
    assert(decoded_pdu.service_tag.tag_number == 0U);
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
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
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
    assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
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
//     assert(decoded_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
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
int main(void)
{
    test_wire_frame_builder_information_report_roundtrip();
    test_association_response_frame_smoke();
    test_wire_frame_builder_aarq_association_roundtrip();
    test_mms_confirmed_request_roundtrip_with_allocated_invoke_id();
    test_mms_read_request_wire_frame_builder_roundtrip();
    test_mms_write_request_wire_frame_builder_roundtrip();
    test_mms_get_variable_access_attributes_request_wire_frame_builder_roundtrip();
    test_mms_get_name_list_request_wire_frame_builder_roundtrip();
    return 0;
}
