#include <assert.h>

#include "server/unitlab_mms_server_runtime.h"
#include "server/unitlab_mms_server_runtime_internal.h"
#include "model/model_plan.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_association_frame.h"
#include "wire/transport/unitlab_mms_transport_frame.h"
#include "wire/session/unitlab_mms_session_spdu.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"
#include "protocols/mms/unitlab_mms_semantic_pdu.h"
#include "protocols/mms/unitlab_mms_core.h"
#include "protocols/mms/unitlab_mms_wire_semantic_bridge.h"

#include <string.h>
#include <stdio.h>

static UnitLabMmsPdu make_information_report_pdu(void)
{
    UnitLabMmsPdu pdu;

    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    pdu.has_service = 1;
    pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    return pdu;
}

static int build_information_report_association_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t scratch[256U];
    return unitlab_mms_build_information_report_frame("RPT", 0U, scratch, sizeof(scratch), buffer, buffer_length, encoded_length, diagnostic);
}

static int find_bytes_offset(const uint8_t* haystack, size_t haystack_length, const uint8_t* needle, size_t needle_length, size_t* offset)
{
    if (offset != NULL) {
        *offset = 0U;
    }
    if (haystack == NULL || needle == NULL || needle_length == 0U || haystack_length < needle_length) {
        return 0;
    }
    for (size_t index = 0U; index + needle_length <= haystack_length; index++) {
        if (memcmp(&haystack[index], needle, needle_length) == 0) {
            if (offset != NULL) {
                *offset = index;
            }
            return 1;
        }
    }
    return 0;
}

static int build_data_change_report_plan(UnitLabIedModelPlan* plan);
static int build_two_model_backed_rcb_plan(UnitLabIedModelPlan* plan);

static int contains_bytes(const uint8_t* haystack, size_t haystack_length, const uint8_t* needle, size_t needle_length)
{
    return find_bytes_offset(haystack, haystack_length, needle, needle_length, NULL);
}

static void assert_ber_tag(const UnitLabMmsBerElement* element, UnitLabMmsBerTagClass tag_class, int constructed, uint32_t tag_number)
{
    assert(element != NULL);
    assert(element->tag.tag_class == tag_class);
    assert(element->tag.constructed == constructed);
    assert(element->tag.tag_number == tag_number);
}

static void assert_information_report_wire_envelope(const uint8_t* report_bytes, size_t report_length)
{
    UnitLabMmsAssociationFrame report_frame;
    UnitLabMmsPdu report_pdu;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed_length = 0U;
    uint16_t tpkt_length = 0U;

    assert(report_bytes != NULL);
    assert(report_length >= 7U);
    assert(report_bytes[0] == 0x03U);
    assert(report_bytes[1] == 0x00U);
    tpkt_length = (uint16_t)(((uint16_t)report_bytes[2] << 8U) | report_bytes[3]);
    assert(tpkt_length == report_length);
    assert(report_bytes[4] == 0x02U);
    assert(report_bytes[5] == 0xF0U);
    assert(report_bytes[6] == 0x80U);

    unitlab_mms_association_frame_init(&report_frame);
    assert(unitlab_mms_association_frame_decode(&report_frame, report_bytes, report_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_length);
    assert(report_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);

    unitlab_mms_pdu_init(&report_pdu);
    assert(unitlab_mms_pdu_decode(&report_pdu, report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_frame.presentation.payload_length);
    assert(report_pdu.kind == UNITLAB_MMS_PDU_UNCONFIRMED);
    assert(report_pdu.service_kind == UNITLAB_MMS_SERVICE_INFORMATION_REPORT);
    assert(report_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(report_pdu.service_tag.constructed == 1);
    assert(report_pdu.service_tag.tag_number == 0U);
}

static void assert_read_response_success_visible_string(const uint8_t* response_bytes, size_t response_length, uint32_t expected_invoke_id, const char* expected_value)
{
    UnitLabMmsAssociationFrame response_frame;
    UnitLabMmsPdu decoded_response_pdu;
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement list_of_access_result_element;
    UnitLabMmsBerElement access_result_element;
    UnitLabMmsBerElement data_element;
    UnitLabMmsDiagnostic diagnostic;
    size_t response_consumed_length = 0U;
    size_t response_pdu_consumed_length = 0U;
    size_t consumed_length = 0U;
    size_t data_consumed_length = 0U;

    assert(response_bytes != NULL);
    unitlab_mms_diagnostic_clear(&diagnostic);

    unitlab_mms_association_frame_init(&response_frame);
    assert(unitlab_mms_association_frame_decode(&response_frame, response_bytes, response_length, &response_consumed_length, &diagnostic) == 1);
    assert(response_consumed_length == response_length);
    assert(response_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);

    unitlab_mms_pdu_init(&decoded_response_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_response_pdu, response_frame.presentation.payload_bytes, response_frame.presentation.payload_length, &response_pdu_consumed_length, &diagnostic) == 1);
    assert(response_pdu_consumed_length == response_frame.presentation.payload_length);
    assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
    assert(decoded_response_pdu.has_invoke_id == 1);
    assert(decoded_response_pdu.invoke_id == expected_invoke_id);
    assert(decoded_response_pdu.has_service == 1);
    assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);

    unitlab_mms_ber_element_init(&invoke_id_element);
    assert(unitlab_mms_ber_read(&invoke_id_element, decoded_response_pdu.pdu_bytes, decoded_response_pdu.pdu_length, &consumed_length, &diagnostic) == 1);
    assert_ber_tag(&invoke_id_element, UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U);
    assert(invoke_id_element.value_length > 0U);
    assert(invoke_id_element.value_bytes[invoke_id_element.value_length - 1U] == (uint8_t)expected_invoke_id);

    unitlab_mms_ber_element_init(&list_of_access_result_element);
    assert(unitlab_mms_ber_read(&list_of_access_result_element, decoded_response_pdu.service_bytes, decoded_response_pdu.service_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == decoded_response_pdu.service_length);
    assert_ber_tag(&list_of_access_result_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U);

    unitlab_mms_ber_element_init(&access_result_element);
    assert(unitlab_mms_ber_read(&access_result_element, list_of_access_result_element.value_bytes, list_of_access_result_element.value_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == list_of_access_result_element.value_length);
    (void)data_element;
    (void)data_consumed_length;
    assert_ber_tag(&access_result_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 10U);
    assert(contains_bytes(access_result_element.value_bytes, access_result_element.value_length, (const uint8_t*)expected_value, strlen(expected_value)) == 1);
}

static void assert_read_response_success_rcb_structure(
    const uint8_t* response_bytes,
    size_t response_length,
    uint32_t expected_invoke_id)
{
    static const uint32_t expected_tags[] = { 10U, 3U, 10U, 6U, 4U, 6U, 6U, 4U, 6U, 3U, 3U, 9U, 12U, 5U };
    UnitLabMmsAssociationFrame response_frame;
    UnitLabMmsPdu decoded_response_pdu;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement list_of_access_result_element;
    UnitLabMmsBerElement access_result_element;
    UnitLabMmsBerElement child_element;
    size_t response_consumed_length = 0U;
    size_t response_pdu_consumed_length = 0U;
    size_t consumed_length = 0U;
    size_t child_offset = 0U;
    size_t child_count = 0U;

    assert(response_bytes != NULL);
    unitlab_mms_diagnostic_clear(&diagnostic);

    unitlab_mms_association_frame_init(&response_frame);
    assert(unitlab_mms_association_frame_decode(&response_frame, response_bytes, response_length, &response_consumed_length, &diagnostic) == 1);
    assert(response_consumed_length == response_length);
    assert(response_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);

    unitlab_mms_pdu_init(&decoded_response_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_response_pdu, response_frame.presentation.payload_bytes, response_frame.presentation.payload_length, &response_pdu_consumed_length, &diagnostic) == 1);
    assert(response_pdu_consumed_length == response_frame.presentation.payload_length);
    assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
    assert(decoded_response_pdu.has_invoke_id == 1);
    assert(decoded_response_pdu.invoke_id == expected_invoke_id);
    assert(decoded_response_pdu.has_service == 1);
    assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);

    unitlab_mms_ber_element_init(&list_of_access_result_element);
    assert(unitlab_mms_ber_read(&list_of_access_result_element, decoded_response_pdu.service_bytes, decoded_response_pdu.service_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == decoded_response_pdu.service_length);
    assert_ber_tag(&list_of_access_result_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U);

    unitlab_mms_ber_element_init(&access_result_element);
    assert(unitlab_mms_ber_read(&access_result_element, list_of_access_result_element.value_bytes, list_of_access_result_element.value_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == list_of_access_result_element.value_length);
    assert_ber_tag(&access_result_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 2U);
    assert(access_result_element.value_bytes[0] != 0x30U);

    while (child_offset < access_result_element.value_length) {
        size_t child_consumed_length = 0U;

        unitlab_mms_ber_element_init(&child_element);
        assert(unitlab_mms_ber_read(&child_element, &access_result_element.value_bytes[child_offset], access_result_element.value_length - child_offset, &child_consumed_length, &diagnostic) == 1);
        assert(child_count < sizeof(expected_tags) / sizeof(expected_tags[0]));
        assert_ber_tag(&child_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, expected_tags[child_count]);
        child_offset += child_consumed_length;
        child_count++;
    }
    assert(child_count == sizeof(expected_tags) / sizeof(expected_tags[0]));
    assert(contains_bytes(access_result_element.value_bytes, access_result_element.value_length, (const uint8_t*)"IED1LD0/LLN0.BR.Events", strlen("IED1LD0/LLN0.BR.Events")) == 1);
    assert(contains_bytes(access_result_element.value_bytes, access_result_element.value_length, (const uint8_t*)"IED1LD0/LLN0$dsEvents", strlen("IED1LD0/LLN0$dsEvents")) == 1);
    assert(contains_bytes(access_result_element.value_bytes, access_result_element.value_length, (const uint8_t*)"Owner", strlen("Owner")) == 0);
}

static void assert_read_response_list_of_access_results_count(const uint8_t* response_bytes, size_t response_length, uint32_t expected_invoke_id, size_t expected_result_count, const char* const* expected_values)
{
    UnitLabMmsAssociationFrame response_frame;
    UnitLabMmsPdu decoded_response_pdu;
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement list_of_access_result_element;
    UnitLabMmsBerElement access_result_element;
    UnitLabMmsBerElement data_element;
    UnitLabMmsDiagnostic diagnostic;
    size_t response_consumed_length = 0U;
    size_t response_pdu_consumed_length = 0U;
    size_t consumed_length = 0U;
    size_t service_offset = 0U;
    size_t actual_result_count = 0U;

    assert(response_bytes != NULL);
    assert(expected_values != NULL);
    unitlab_mms_diagnostic_clear(&diagnostic);

    unitlab_mms_association_frame_init(&response_frame);
    assert(unitlab_mms_association_frame_decode(&response_frame, response_bytes, response_length, &response_consumed_length, &diagnostic) == 1);
    assert(response_consumed_length == response_length);
    assert(response_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);

    unitlab_mms_pdu_init(&decoded_response_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_response_pdu, response_frame.presentation.payload_bytes, response_frame.presentation.payload_length, &response_pdu_consumed_length, &diagnostic) == 1);
    assert(response_pdu_consumed_length == response_frame.presentation.payload_length);
    assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
    assert(decoded_response_pdu.has_invoke_id == 1);
    assert(decoded_response_pdu.invoke_id == expected_invoke_id);
    assert(decoded_response_pdu.has_service == 1);
    assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);

    unitlab_mms_ber_element_init(&invoke_id_element);
    assert(unitlab_mms_ber_read(&invoke_id_element, decoded_response_pdu.pdu_bytes, decoded_response_pdu.pdu_length, &consumed_length, &diagnostic) == 1);
    assert_ber_tag(&invoke_id_element, UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U);
    assert(invoke_id_element.value_length > 0U);
    assert(invoke_id_element.value_bytes[invoke_id_element.value_length - 1U] == (uint8_t)expected_invoke_id);

    unitlab_mms_ber_element_init(&list_of_access_result_element);
    assert(unitlab_mms_ber_read(&list_of_access_result_element, decoded_response_pdu.service_bytes, decoded_response_pdu.service_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == decoded_response_pdu.service_length);
    assert_ber_tag(&list_of_access_result_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U);

    while (service_offset < list_of_access_result_element.value_length) {
        size_t access_result_consumed_length = 0U;
        size_t data_consumed_length = 0U;

        unitlab_mms_ber_element_init(&access_result_element);
        assert(unitlab_mms_ber_read(&access_result_element, &list_of_access_result_element.value_bytes[service_offset], list_of_access_result_element.value_length - service_offset, &access_result_consumed_length, &diagnostic) == 1);
        assert(access_result_consumed_length > 0U);
        (void)data_element;
        (void)data_consumed_length;
        assert(access_result_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
        assert(access_result_element.tag.tag_number == 10U);
        assert(expected_values[actual_result_count] != NULL);
        assert(contains_bytes(access_result_element.value_bytes, access_result_element.value_length, (const uint8_t*)expected_values[actual_result_count], strlen(expected_values[actual_result_count])) == 1);
        service_offset += access_result_consumed_length;
        actual_result_count++;
    }
    assert(service_offset == list_of_access_result_element.value_length);
    assert(actual_result_count == expected_result_count);
}

static void assert_read_response_access_result_tags(
    const uint8_t* response_bytes,
    size_t response_length,
    uint32_t expected_invoke_id,
    const uint32_t* expected_tags,
    size_t expected_tag_count)
{
    UnitLabMmsAssociationFrame response_frame;
    UnitLabMmsPdu decoded_response_pdu;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement list_of_access_result_element;
    UnitLabMmsBerElement access_result_element;
    size_t response_consumed_length = 0U;
    size_t response_pdu_consumed_length = 0U;
    size_t consumed_length = 0U;
    size_t offset = 0U;
    size_t index = 0U;

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_association_frame_init(&response_frame);
    assert(unitlab_mms_association_frame_decode(&response_frame, response_bytes, response_length, &response_consumed_length, &diagnostic) == 1);
    assert(response_consumed_length == response_length);
    unitlab_mms_pdu_init(&decoded_response_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_response_pdu, response_frame.presentation.payload_bytes, response_frame.presentation.payload_length, &response_pdu_consumed_length, &diagnostic) == 1);
    assert(response_pdu_consumed_length == response_frame.presentation.payload_length);
    assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
    assert(decoded_response_pdu.invoke_id == expected_invoke_id);
    assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);

    unitlab_mms_ber_element_init(&list_of_access_result_element);
    assert(unitlab_mms_ber_read(&list_of_access_result_element, decoded_response_pdu.service_bytes, decoded_response_pdu.service_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == decoded_response_pdu.service_length);
    assert_ber_tag(&list_of_access_result_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U);

    while (offset < list_of_access_result_element.value_length) {
        size_t access_result_consumed_length = 0U;

        unitlab_mms_ber_element_init(&access_result_element);
        assert(unitlab_mms_ber_read(&access_result_element, &list_of_access_result_element.value_bytes[offset], list_of_access_result_element.value_length - offset, &access_result_consumed_length, &diagnostic) == 1);
        assert(index < expected_tag_count);
        assert_ber_tag(&access_result_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, expected_tags[index] == 2U ? 1 : 0, expected_tags[index]);
        offset += access_result_consumed_length;
        index++;
    }
    assert(offset == list_of_access_result_element.value_length);
    assert(index == expected_tag_count);
}

static void assert_read_response_failure_access_result(const uint8_t* response_bytes, size_t response_length, uint32_t expected_invoke_id)
{
    static const uint32_t expected_failure_tag[] = { 0U };
    UnitLabMmsAssociationFrame response_frame;
    UnitLabMmsPdu decoded_response_pdu;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement list_of_access_result_element;
    UnitLabMmsBerElement access_result_element;
    size_t response_consumed_length = 0U;
    size_t response_pdu_consumed_length = 0U;
    size_t consumed_length = 0U;

    (void)expected_failure_tag;
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_association_frame_init(&response_frame);
    assert(unitlab_mms_association_frame_decode(&response_frame, response_bytes, response_length, &response_consumed_length, &diagnostic) == 1);
    unitlab_mms_pdu_init(&decoded_response_pdu);
    assert(unitlab_mms_pdu_decode(&decoded_response_pdu, response_frame.presentation.payload_bytes, response_frame.presentation.payload_length, &response_pdu_consumed_length, &diagnostic) == 1);
    assert(decoded_response_pdu.invoke_id == expected_invoke_id);
    assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
    unitlab_mms_ber_element_init(&list_of_access_result_element);
    assert(unitlab_mms_ber_read(&list_of_access_result_element, decoded_response_pdu.service_bytes, decoded_response_pdu.service_length, &consumed_length, &diagnostic) == 1);
    assert_ber_tag(&list_of_access_result_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U);
    unitlab_mms_ber_element_init(&access_result_element);
    assert(unitlab_mms_ber_read(&access_result_element, list_of_access_result_element.value_bytes, list_of_access_result_element.value_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == list_of_access_result_element.value_length);
    assert_ber_tag(&access_result_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 0U);
    assert(access_result_element.value_length <= 1U);
}

static void assert_named_variable_list_attributes_member(const UnitLabMmsBerElement* member, const char* expected_domain, const char* expected_item)
{
    UnitLabMmsBerElement variable_spec;
    UnitLabMmsBerElement object_name;
    UnitLabMmsBerElement child;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed_length = 0U;
    size_t child_consumed_length = 0U;
    size_t offset = 0U;

    assert(member != NULL);
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert_ber_tag(member, UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 1, 16U);

    unitlab_mms_ber_element_init(&variable_spec);
    assert(unitlab_mms_ber_read(&variable_spec, member->value_bytes, member->value_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == member->value_length);
    assert_ber_tag(&variable_spec, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 0U);
    unitlab_mms_ber_element_init(&object_name);
    assert(unitlab_mms_ber_read(&object_name, variable_spec.value_bytes, variable_spec.value_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == variable_spec.value_length);
    assert_ber_tag(&object_name, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U);

    unitlab_mms_ber_element_init(&child);
    assert(unitlab_mms_ber_read(&child, object_name.value_bytes, object_name.value_length, &child_consumed_length, &diagnostic) == 1);
    assert_ber_tag(&child, UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 26U);
    assert(child.value_length == strlen(expected_domain));
    assert(memcmp(child.value_bytes, expected_domain, child.value_length) == 0);
    offset += child_consumed_length;

    unitlab_mms_ber_element_init(&child);
    assert(unitlab_mms_ber_read(&child, &object_name.value_bytes[offset], object_name.value_length - offset, &child_consumed_length, &diagnostic) == 1);
    assert_ber_tag(&child, UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 26U);
    assert(child.value_length == strlen(expected_item));
    assert(memcmp(child.value_bytes, expected_item, child.value_length) == 0);
    offset += child_consumed_length;
    assert(offset == object_name.value_length);
}

static void assert_named_variable_list_attributes_response_shape(const UnitLabMmsPdu* response_pdu, const char* expected_member_token_0, const char* expected_member_token_1, const char* expected_member_token_2, size_t expected_member_token_count)
{
    UnitLabMmsBerElement deletable_element;
    UnitLabMmsBerElement list_of_variable_element;
    UnitLabMmsBerElement member;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed_length = 0U;
    size_t member_consumed_length = 0U;
    size_t offset = 0U;

    assert(response_pdu != NULL);
    unitlab_mms_diagnostic_clear(&diagnostic);

    unitlab_mms_ber_element_init(&deletable_element);
    assert(unitlab_mms_ber_read(&deletable_element, response_pdu->service_bytes, response_pdu->service_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length <= response_pdu->service_length);
    assert_ber_tag(&deletable_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 0U);
    assert(deletable_element.value_length == 1U);
    assert(deletable_element.value_bytes[0] == 0x00U);

    unitlab_mms_ber_element_init(&list_of_variable_element);
    assert(unitlab_mms_ber_read(&list_of_variable_element, &response_pdu->service_bytes[consumed_length], response_pdu->service_length - consumed_length, &consumed_length, &diagnostic) == 1);
    assert_ber_tag(&list_of_variable_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U);

    unitlab_mms_ber_element_init(&member);
    assert(unitlab_mms_ber_read(&member, list_of_variable_element.value_bytes, list_of_variable_element.value_length, &member_consumed_length, &diagnostic) == 1);
    assert(member_consumed_length > 0U);
    assert_named_variable_list_attributes_member(&member, "LD0", expected_member_token_0);
    offset += member_consumed_length;

    if (expected_member_token_count > 1U) {
        unitlab_mms_ber_element_init(&member);
        assert(unitlab_mms_ber_read(&member, &list_of_variable_element.value_bytes[offset], list_of_variable_element.value_length - offset, &member_consumed_length, &diagnostic) == 1);
        assert(member_consumed_length > 0U);
        assert_named_variable_list_attributes_member(&member, "LD0", expected_member_token_1);
        offset += member_consumed_length;
    }
    if (expected_member_token_count > 2U) {
        unitlab_mms_ber_element_init(&member);
        assert(unitlab_mms_ber_read(&member, &list_of_variable_element.value_bytes[offset], list_of_variable_element.value_length - offset, &member_consumed_length, &diagnostic) == 1);
        assert(member_consumed_length > 0U);
        assert_named_variable_list_attributes_member(&member, "LD0", expected_member_token_2);
        offset += member_consumed_length;
    }
    assert(offset == list_of_variable_element.value_length);
}


static void test_server_runtime_rptena_write_updates_brcb_read_state(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x2AU;
    UnitLabMmsBerElement data_element;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$ResvTms", &data_element, 19U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.brcb_resv_tms == 42U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    value_byte = 0x01U;
    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$RptEna", &data_element, 20U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.brcb_rpt_ena == 1U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_build_read_request_frame("IED1LD0", "LLN0$BR$brcbEvents", 21U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"IED1LD0/LLN0.BR.Events", strlen("IED1LD0/LLN0.BR.Events")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"IED1LD0/LLN0$dsEvents", strlen("IED1LD0/LLN0$dsEvents")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x83\x01\x01", 3U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x85\x01\x2A", 3U) == 1);

    unitlab_free_ied_model_plan(&plan);
}


static void test_server_runtime_resvtms_owner_tracks_reservation_lifecycle(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x2AU;
    UnitLabMmsBerElement data_element;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$ResvTms", &data_element, 36U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.brcb_resv_tms == 42U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED);
    assert(strcmp(server_runtime.brcb_owner, "local-client") == 0);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 37U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "IED1LD0.LLN0.BR.brcbEvents.Owner");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "Owner");
    server_runtime.pending_request.read_object_reference_count = 1U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.LLN0.BR.brcbEvents.Owner");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "Owner");
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x8A\x0Clocal-client", 14U) == 1);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    value_byte = 0x00U;
    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$ResvTms", &data_element, 38U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.brcb_resv_tms == 0U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(server_runtime.brcb_owner[0] == '\0');
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 39U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "IED1LD0.LLN0.BR.brcbEvents.Owner");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "Owner");
    server_runtime.pending_request.read_object_reference_count = 1U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.LLN0.BR.brcbEvents.Owner");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "Owner");
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x8A\x00", 2U) == 1);
}

static void test_server_runtime_resvtms_no_br_alias_read_write_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x2AU;
    UnitLabMmsBerElement data_element;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    static const uint32_t expected_integer_tag[1U] = { 5U };

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    assert(unitlab_mms_build_read_request_frame("IED1LD0", "LLN0.brcbEvents", 29U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(strcmp(server_runtime.pending_request.object_reference, "IED1LD0.LLN0.brcbEvents") == 0);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert_read_response_success_rcb_structure(response_bytes, response_length, 29U);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_build_read_request_frame("IED1LD0", "LLN0.brcbEvents.ResvTms", 26U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(strcmp(server_runtime.pending_request.object_reference, "IED1LD0.LLN0.brcbEvents.ResvTms") == 0);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert_read_response_access_result_tags(response_bytes, response_length, 26U, expected_integer_tag, 1U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x85\x01\x00", 3U) == 1);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0.brcbEvents.ResvTms", &data_element, 27U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(strcmp(server_runtime.pending_request.object_reference, "IED1LD0.LLN0.brcbEvents.ResvTms") == 0);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.brcb_resv_tms == 42U);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_build_read_request_frame("IED1LD0", "LLN0$brcbEvents$ResvTms", 28U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(strcmp(server_runtime.pending_request.object_reference, "IED1LD0.LLN0.brcbEvents.ResvTms") == 0);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert_read_response_access_result_tags(response_bytes, response_length, 28U, expected_integer_tag, 1U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x85\x01\x2A", 3U) == 1);
}


static void test_server_runtime_reads_model_backed_rcb_dataset_aliases(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[8192U];
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_two_model_backed_rcb_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    assert(unitlab_mms_build_read_request_frame("KINTE08TDIFFSystem", "LLN0$BR", 194U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(response_length > 0U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0.brcbA", strlen("KINTE08TDIFFSystem/LLN0.brcbA")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0.brcbB", strlen("KINTE08TDIFFSystem/LLN0.brcbB")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB1", strlen("KINTE08TDIFFSystem/LLN0$RCB1")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB2", strlen("KINTE08TDIFFSystem/LLN0$RCB2")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"brcbEvents", strlen("brcbEvents")) == 0);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    response_length = 0U;
    consumed_length = 0U;
    assert(unitlab_mms_build_read_request_frame("KINTE08TDIFFSystem", "LLN0$RCB1", 195U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(response_length > 0U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0.brcbA", strlen("KINTE08TDIFFSystem/LLN0.brcbA")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0.brcbB", strlen("KINTE08TDIFFSystem/LLN0.brcbB")) == 0);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB1", strlen("KINTE08TDIFFSystem/LLN0$RCB1")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB2", strlen("KINTE08TDIFFSystem/LLN0$RCB2")) == 0);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_build_read_request_frame("KINTE08TDIFFSystem", "LLN0$RCB2", 196U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(response_length > 0U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0.brcbA", strlen("KINTE08TDIFFSystem/LLN0.brcbA")) == 0);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0.brcbB", strlen("KINTE08TDIFFSystem/LLN0.brcbB")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB1", strlen("KINTE08TDIFFSystem/LLN0$RCB1")) == 0);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB2", strlen("KINTE08TDIFFSystem/LLN0$RCB2")) == 1);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_build_read_request_frame("KINTE08TDIFFSystem", "LLN0$RCB2$Owner", 197U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x8A\x00", 2U) == 1);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    unitlab_free_ied_model_plan(&plan);
}



static void test_server_runtime_reads_model_backed_urcb_class_container(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[8192U];
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_two_model_backed_rcb_plan(&plan) == 1);
    for (size_t index = 0U; index < plan.report_count; index++) {
        plan.reports[index].is_buffered = 0;
        snprintf(plan.reports[index].report_kind, sizeof(plan.reports[index].report_kind), "%s", "unbuffered");
        snprintf(plan.reports[index].rpt_id, sizeof(plan.reports[index].rpt_id), "KINTE08TDIFFSystem/LLN0.urcb%c", (char)('A' + index));
    }

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    assert(unitlab_mms_build_read_request_frame("KINTE08TDIFFSystem", "LLN0$RP", 198U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(response_length > 0U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0.urcbA", strlen("KINTE08TDIFFSystem/LLN0.urcbA")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0.urcbB", strlen("KINTE08TDIFFSystem/LLN0.urcbB")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB1", strlen("KINTE08TDIFFSystem/LLN0$RCB1")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB2", strlen("KINTE08TDIFFSystem/LLN0$RCB2")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"object-access-unsupported", strlen("object-access-unsupported")) == 0);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_second_model_rcb_gi_uses_second_dataset(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t scratch[1024U];
    uint8_t request_bytes[1024U];
    uint8_t response_bytes[2048U];
    uint8_t report_bytes[8192U];
    uint8_t value_byte = 0x01U;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t report_length = 0U;
    UnitLabMmsBerElement data_element;
    UnitLabMmsAssociationFrame report_frame;

    assert(build_two_model_backed_rcb_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;

    assert(unitlab_mms_build_write_request_frame("KINTE08TDIFFSystem", "LLN0$BR$brcbB$RptEna", &data_element, 210U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.active_report_index == 1U);
    assert(server_runtime.brcb_rpt_ena == 1U);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_build_write_request_frame("KINTE08TDIFFSystem", "LLN0$BR$brcbB$GI", &data_element, 211U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.active_report_index == 1U);
    assert(unitlab_mms_server_runtime_has_pending_gi_report(&server_runtime) == 1);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    unitlab_mms_association_frame_init(&report_frame);
    assert(unitlab_mms_association_frame_decode(&report_frame, report_bytes, report_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_length);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB2", strlen("KINTE08TDIFFSystem/LLN0$RCB2")) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB1", strlen("KINTE08TDIFFSystem/LLN0$RCB1")) == 0);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_gi_write_queues_information_report(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t report_bytes[2048U];
    uint8_t value_byte = 0x01U;
    UnitLabMmsBerElement data_element;
    UnitLabMmsAssociationFrame report_frame;
    UnitLabMmsPdu report_pdu;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t report_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$GI", &data_element, 23U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(unitlab_mms_server_runtime_has_pending_gi_report(&server_runtime) == 1);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(unitlab_mms_server_runtime_has_pending_gi_report(&server_runtime) == 0);
    assert_information_report_wire_envelope(report_bytes, report_length);

    unitlab_mms_association_frame_init(&report_frame);
    assert(unitlab_mms_association_frame_decode(&report_frame, report_bytes, report_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_length);
    assert(report_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    unitlab_mms_pdu_init(&report_pdu);
    assert(unitlab_mms_pdu_decode(&report_pdu, report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_frame.presentation.payload_length);
    assert(report_pdu.kind == UNITLAB_MMS_PDU_UNCONFIRMED);
    assert(report_pdu.service_kind == UNITLAB_MMS_SERVICE_INFORMATION_REPORT);
    assert(report_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(report_pdu.service_tag.constructed == 1);
    assert(report_pdu.service_tag.tag_number == 0U);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1LD0/LLN0.BR.Events", strlen("IED1LD0/LLN0.BR.Events")) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1LD0/LLN0$dsEvents", strlen("IED1LD0/LLN0$dsEvents")) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1LD0/XCBR1$ST$Pos$stVal", strlen("IED1LD0/XCBR1$ST$Pos$stVal")) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x84\x02\x06\xC0", 4U) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x84\x02\x02\x04", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}


static void test_server_runtime_gi_report_uses_model_dataset_members(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedFixtureSignal signals[3U] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "1",
        },
        {
            .data_set_index = 1U,
            .reference = "LD0/PGGIO1.Ind1.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "2",
        },
        {
            .data_set_index = 2U,
            .reference = "LD1/GGIO1.Ind2.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "3",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1U] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 3U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1U] = {
        {
            .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
            .logical_device_inst = "LD0",
            .logical_node_name = "LLN0",
            .report_control_name = "brcbEvents",
            .report_kind = "buffered",
            .rpt_id = "IED1LD0/LLN0.BR.Events",
            .data_set_ref = "IED1/AP1/LD0/LLN0.dsEvents",
            .conf_rev = "11",
            .indexed_known = 1,
            .indexed = 0,
            .buffer_time_ms_known = 1,
            .buffer_time_ms = 100,
            .integrity_period_ms_known = 1,
            .integrity_period_ms = 1000,
        },
    };
    reports[0].optional_fields.sequence_number = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.timestamp = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.reason_code = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_set_name = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_reference = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.buffer_overflow = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.entry_id = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.config_revision = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };

    UnitLabIedFixtureModel fixture = {
        .device_count = 1U,
        .ied_name = "IED1",
        .access_point_name = "AP1",
        .data_set_count = 1U,
        .data_sets = data_sets,
        .report_count = 1U,
        .reports = reports,
        .signal_count = 3U,
    };
    UnitLabIedModelPlan plan;
    char error[256U];
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t report_bytes[4096U];
    uint8_t value_byte = 0x01U;
    UnitLabMmsBerElement data_element;
    UnitLabMmsAssociationFrame report_frame;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t report_length = 0U;

    assert(unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error)) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$GI", &data_element, 24U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(server_runtime.brcb_sq_num == 1U);
    assert(server_runtime.brcb_entry_id_counter > 1000000000000ULL);
    assert(contains_bytes(server_runtime.brcb_entry_id, sizeof(server_runtime.brcb_entry_id), (const uint8_t*)"\x01", 1U) == 1);

    unitlab_mms_association_frame_init(&report_frame);
    assert(unitlab_mms_association_frame_decode(&report_frame, report_bytes, report_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_length);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1LD1/GGIO1$ST$Ind2$stVal", strlen("IED1LD1/GGIO1$ST$Ind2$stVal")) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1LD0/GGIO1$ST$Ind2$stVal", strlen("IED1LD0/GGIO1$ST$Ind2$stVal")) == 0);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1IED1LD0", strlen("IED1IED1LD0")) == 0);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x86\x01\x0B", 3U) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x84\x02\x05\xE0", 4U) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x85\x01\x03", 3U) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x89\x08", 2U) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x8C\x06", 2U) == 1);

    unitlab_free_ied_model_plan(&plan);
}


static int build_data_change_report_plan(UnitLabIedModelPlan* plan)
{
    UnitLabIedFixtureSignal signals[2U] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .component = "stVal",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
        {
            .data_set_index = 1U,
            .reference = "LD0/PGGIO1.Ind1.stVal[ST]",
            .kind = "FCDA",
            .component = "stVal",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1U] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 2U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1U] = {
        {
            .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
            .logical_device_inst = "LD0",
            .logical_node_name = "LLN0",
            .report_control_name = "brcbEvents",
            .report_kind = "buffered",
            .rpt_id = "IED1LD0/LLN0.BR.Events",
            .data_set_ref = "IED1/AP1/LD0/LLN0.dsEvents",
            .conf_rev = "7",
            .indexed_known = 1,
            .indexed = 0,
            .buffer_time_ms_known = 1,
            .buffer_time_ms = 100,
            .integrity_period_ms_known = 1,
            .integrity_period_ms = 1000,
        },
    };
    reports[0].optional_fields.sequence_number = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.timestamp = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.reason_code = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_set_name = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_reference = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.buffer_overflow = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.entry_id = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.config_revision = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };

    UnitLabIedFixtureModel fixture = {
        .device_count = 1U,
        .ied_name = "IED1",
        .access_point_name = "AP1",
        .data_set_count = 1U,
        .data_sets = data_sets,
        .report_count = 1U,
        .reports = reports,
        .signal_count = 2U,
    };
    char error[256U];

    return unitlab_build_ied_model_plan(&fixture, plan, error, sizeof(error));
}



static int build_two_model_backed_rcb_plan(UnitLabIedModelPlan* plan)
{
    UnitLabIedFixtureSignal signals[2U] = {
        {
            .data_set_index = 0U,
            .reference = "System/LLN0.Mod.stVal[ST]",
            .kind = "FCDA",
            .component = "stVal",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
        {
            .data_set_index = 0U,
            .reference = "Protection/LLN0.Beh.stVal[ST]",
            .kind = "FCDA",
            .component = "stVal",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[2U] = {
        {
            .reference = "KINTE08TDIFF/AP1/System/LLN0.RCB1",
            .signal_count = 1U,
            .signals = &signals[0],
        },
        {
            .reference = "KINTE08TDIFF/AP1/System/LLN0.RCB2",
            .signal_count = 1U,
            .signals = &signals[1],
        },
    };
    UnitLabIedFixtureReport reports[2U] = {
        {
            .key = "KINTE08TDIFF/AP1/System/LLN0/brcbA/buffered",
            .logical_device_inst = "System",
            .logical_node_name = "LLN0",
            .report_control_name = "brcbA",
            .report_kind = "buffered",
            .rpt_id = "KINTE08TDIFFSystem/LLN0.brcbA",
            .data_set_ref = "KINTE08TDIFF/AP1/System/LLN0.RCB1",
            .conf_rev = "10001",
            .indexed_known = 1,
            .indexed = 1,
            .buffer_time_ms_known = 1,
            .buffer_time_ms = 500,
            .integrity_period_ms_known = 1,
            .integrity_period_ms = 0,
        },
        {
            .key = "KINTE08TDIFF/AP1/System/LLN0/brcbB/buffered",
            .logical_device_inst = "System",
            .logical_node_name = "LLN0",
            .report_control_name = "brcbB",
            .report_kind = "buffered",
            .rpt_id = "KINTE08TDIFFSystem/LLN0.brcbB",
            .data_set_ref = "KINTE08TDIFF/AP1/System/LLN0.RCB2",
            .conf_rev = "10001",
            .indexed_known = 1,
            .indexed = 1,
            .buffer_time_ms_known = 1,
            .buffer_time_ms = 500,
            .integrity_period_ms_known = 1,
            .integrity_period_ms = 0,
        },
    };
    UnitLabIedFixtureModel fixture = {
        .device_count = 1U,
        .ied_name = "KINTE08TDIFF",
        .access_point_name = "AP1",
        .data_set_count = 2U,
        .data_sets = data_sets,
        .report_count = 2U,
        .reports = reports,
        .signal_count = 2U,
    };
    char error[256U];

    for (size_t index = 0U; index < 2U; index++) {
        reports[index].optional_fields.sequence_number = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
        reports[index].optional_fields.timestamp = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
        reports[index].optional_fields.reason_code = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
        reports[index].optional_fields.data_set_name = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
        reports[index].optional_fields.data_reference = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
        reports[index].optional_fields.buffer_overflow = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
        reports[index].optional_fields.entry_id = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
        reports[index].optional_fields.config_revision = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    }

    return unitlab_build_ied_model_plan(&fixture, plan, error, sizeof(error));
}


static int build_runtime_value_store_plan(UnitLabIedModelPlan* plan)
{
    UnitLabIedFixtureSignal event_signals[2U] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .component = "stVal",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
        {
            .data_set_index = 1U,
            .reference = "LD0/PGGIO1.Ind1.stVal[ST]",
            .kind = "FCDA",
            .component = "stVal",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureSignal wire_signals[1U] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/GGIO1.AnIn1.mag.f[MX]",
            .kind = "FCDA",
            .component = "f",
            .fc = "MX",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[2U] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 2U,
            .signals = event_signals,
        },
        {
            .reference = "IED1/AP1/LD0/GGIO1.dsWire",
            .signal_count = 1U,
            .signals = wire_signals,
        },
    };
    UnitLabIedFixtureReport reports[1U] = {
        {
            .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
            .logical_device_inst = "LD0",
            .logical_node_name = "LLN0",
            .report_control_name = "brcbEvents",
            .report_kind = "buffered",
            .rpt_id = "IED1LD0/LLN0.BR.Events",
            .data_set_ref = "IED1/AP1/LD0/LLN0.dsEvents",
            .conf_rev = "7",
            .indexed_known = 1,
            .indexed = 0,
            .buffer_time_ms_known = 1,
            .buffer_time_ms = 100,
            .integrity_period_ms_known = 1,
            .integrity_period_ms = 1000,
        },
    };
    UnitLabIedFixtureModel fixture = {
        .device_count = 1U,
        .ied_name = "IED1",
        .access_point_name = "AP1",
        .data_set_count = 2U,
        .data_sets = data_sets,
        .report_count = 1U,
        .reports = reports,
        .signal_count = 3U,
    };
    char error[256U];

    reports[0].optional_fields.sequence_number = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.timestamp = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.reason_code = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_set_name = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_reference = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.buffer_overflow = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.entry_id = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.config_revision = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    return unitlab_build_ied_model_plan(&fixture, plan, error, sizeof(error));
}

static int build_runtime_quality_timestamp_plan(UnitLabIedModelPlan* plan)
{
    UnitLabIedFixtureSignal signals[3U] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/PGGIO1.Ind1.stVal[ST]",
            .kind = "FCDA",
            .component = "stVal",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_BOOLEAN,
            .initial_value = "false",
        },
        {
            .data_set_index = 1U,
            .reference = "LD0/PGGIO1.Ind1.q[ST]",
            .kind = "FCDA",
            .component = "q",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
        {
            .data_set_index = 2U,
            .reference = "LD0/PGGIO1.Ind1.t[ST]",
            .kind = "FCDA",
            .component = "t",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1U] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 3U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1U] = {
        {
            .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
            .logical_device_inst = "LD0",
            .logical_node_name = "LLN0",
            .report_control_name = "brcbEvents",
            .report_kind = "buffered",
            .rpt_id = "IED1LD0/LLN0.BR.Events",
            .data_set_ref = "IED1/AP1/LD0/LLN0.dsEvents",
            .conf_rev = "7",
            .indexed_known = 1,
            .indexed = 0,
            .buffer_time_ms_known = 1,
            .buffer_time_ms = 100,
            .integrity_period_ms_known = 1,
            .integrity_period_ms = 1000,
        },
    };
    UnitLabIedFixtureModel fixture = {
        .device_count = 1U,
        .ied_name = "IED1",
        .access_point_name = "AP1",
        .data_set_count = 1U,
        .data_sets = data_sets,
        .report_count = 1U,
        .reports = reports,
        .signal_count = 3U,
    };
    char error[256U];

    reports[0].optional_fields.sequence_number = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.timestamp = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.reason_code = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_set_name = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_reference = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.buffer_overflow = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.entry_id = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.config_revision = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    return unitlab_build_ied_model_plan(&fixture, plan, error, sizeof(error));
}

static void test_server_runtime_updates_quality_timestamp_next_to_value_leaf(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t response_bytes[1024U];
    const uint8_t quality_bytes[2U] = { 0x12U, 0x34U };
    const uint8_t timestamp_bytes[8U] = { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U };
    size_t response_length = 0U;

    assert(build_runtime_quality_timestamp_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_boolean(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", 1, &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_quality(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", quality_bytes, sizeof(quality_bytes), &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_timestamp(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", timestamp_bytes, sizeof(timestamp_bytes), &diagnostic));

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 64U, 7U, 1000U, 100U, &diagnostic) == 1);
    server_runtime.pending_request.read_object_reference_count = 3U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.PGGIO1.ST.Ind1.stVal");
    snprintf(server_runtime.pending_request.read_object_references[1], sizeof(server_runtime.pending_request.read_object_references[1]), "%s", "IED1LD0.PGGIO1.ST.Ind1.q");
    snprintf(server_runtime.pending_request.read_object_references[2], sizeof(server_runtime.pending_request.read_object_references[2]), "%s", "IED1LD0.PGGIO1.ST.Ind1.t");

    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x83\x01\xFF", 3U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x02\x12\x34", 4U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x91\x08\x01\x02\x03\x04\x05\x06\x07\x08", 10U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_queued_report_uses_event_value_snapshot(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t first_report_bytes[4096U];
    uint8_t second_report_bytes[4096U];
    const uint8_t first_quality[2U] = { 0x11U, 0x22U };
    const uint8_t later_quality[2U] = { 0x33U, 0x44U };
    size_t first_report_length = 0U;
    size_t second_report_length = 0U;

    assert(build_runtime_quality_timestamp_plan(&plan) == 1);
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI;
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_boolean(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", 1, &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_quality(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", first_quality, sizeof(first_quality), &diagnostic));
    assert(server_runtime.pending_report_queue_count == 1U);

    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, first_report_bytes, sizeof(first_report_bytes), &first_report_length, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE);

    server_runtime.brcb_rpt_ena = 0U;
    assert(unitlab_mms_server_runtime_update_signal_quality(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", later_quality, sizeof(later_quality), &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, second_report_bytes, sizeof(second_report_bytes), &second_report_length, &diagnostic));
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"\x84\x02\x11\x22", 4U) == 1);
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"\x84\x02\x33\x44", 4U) == 0);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_different_report_kinds_are_queued_in_order(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t first_report_bytes[4096U];
    uint8_t second_report_bytes[4096U];
    const uint8_t quality_bytes[2U] = { 0x55U, 0x66U };
    size_t first_report_length = 0U;
    size_t second_report_length = 0U;

    assert(build_runtime_quality_timestamp_plan(&plan) == 1);
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI;
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_boolean(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", 1, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert(unitlab_mms_server_runtime_update_signal_quality(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", quality_bytes, sizeof(quality_bytes), &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert(server_runtime.pending_report_queue_count == 1U);
    assert(server_runtime.pending_report_queue[0].kind == UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE);

    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, first_report_bytes, sizeof(first_report_bytes), &first_report_length, &diagnostic));
    assert(contains_bytes(first_report_bytes, first_report_length, (const uint8_t*)"\x84\x02\x02\x80", 4U) == 1);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE);
    assert(server_runtime.pending_report_queue_count == 0U);

    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, second_report_bytes, sizeof(second_report_bytes), &second_report_length, &diagnostic));
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$q", strlen("IED1LD0/PGGIO1$ST$Ind1$q")) == 1);
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"\x84\x02\x55\x66", 4U) == 1);
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"\x84\x02\x02\x40", 4U) == 1);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_quality_update_queues_quality_change_report(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t report_bytes[4096U];
    const uint8_t quality_bytes[2U] = { 0x31U, 0x42U };
    const uint8_t timestamp_bytes[8U] = { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U };
    size_t report_length = 0U;

    assert(build_runtime_quality_timestamp_plan(&plan) == 1);
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI;
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_timestamp(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", timestamp_bytes, sizeof(timestamp_bytes), &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);

    assert(unitlab_mms_server_runtime_update_signal_quality(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", quality_bytes, sizeof(quality_bytes), &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE);
    assert(server_runtime.pending_report_member_index == 1U);
    assert((server_runtime.pending_report_member_mask & 0x02U) == 0x02U);

    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$q", strlen("IED1LD0/PGGIO1$ST$Ind1$q")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 0);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x31\x42", 4U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x05\x40", 4U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x02\x40", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_quality_timestamp_update_aliases_share_stored_value(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t report_bytes[4096U];
    uint8_t response_bytes[1024U];
    const uint8_t quality_bytes[2U] = { 0x61U, 0x62U };
    const uint8_t timestamp_bytes[8U] = { 0x10U, 0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U };
    size_t report_length = 0U;
    size_t response_length = 0U;

    assert(build_runtime_quality_timestamp_plan(&plan) == 1);
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI;
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_timestamp(&server_runtime, "IED1LD0.PGGIO1.ST.Ind1.stVal", timestamp_bytes, sizeof(timestamp_bytes), &diagnostic));
    assert(server_runtime.signal_value_count == 3U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);
    assert(unitlab_mms_server_runtime_update_signal_quality(&server_runtime, "IED1LD0.PGGIO1.ST.Ind1.stVal", quality_bytes, sizeof(quality_bytes), &diagnostic));
    assert(server_runtime.signal_value_count == 3U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE);
    assert(server_runtime.pending_report_member_index == 1U);
    assert((server_runtime.pending_report_member_mask & 0x02U) == 0x02U);

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 66U, 7U, 1000U, 100U, &diagnostic) == 1);
    server_runtime.pending_request.read_object_reference_count = 2U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.PGGIO1.ST.Ind1.q");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "q");
    snprintf(server_runtime.pending_request.read_object_references[1], sizeof(server_runtime.pending_request.read_object_references[1]), "%s", "IED1LD0.PGGIO1.ST.Ind1.t");
    snprintf(server_runtime.pending_request.read_attribute_references[1], sizeof(server_runtime.pending_request.read_attribute_references[1]), "%s", "t");
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x02\x61\x62", 4U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x91\x08\x10\x11\x12\x13\x14\x15\x16\x17", 10U) == 1);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$q", strlen("IED1LD0/PGGIO1$ST$Ind1$q")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x61\x62", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_gi_report_uses_stored_quality_timestamp_members(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t report_bytes[4096U];
    const uint8_t quality_bytes[2U] = { 0x45U, 0x67U };
    const uint8_t timestamp_bytes[8U] = { 0x10U, 0x20U, 0x30U, 0x40U, 0x50U, 0x60U, 0x70U, 0x80U };
    size_t report_length = 0U;

    assert(build_runtime_quality_timestamp_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_boolean(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", 1, &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_quality(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", quality_bytes, sizeof(quality_bytes), &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_timestamp(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", timestamp_bytes, sizeof(timestamp_bytes), &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));

    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x83\x01\xFF", 3U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x45\x67", 4U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x91\x08\x10\x20\x30\x40\x50\x60\x70\x80", 10U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_data_change_report_uses_stored_quality_timestamp_members(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t report_bytes[4096U];
    const uint8_t quality_bytes[2U] = { 0x7AU, 0xBCU };
    const uint8_t timestamp_bytes[8U] = { 0x21U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U, 0x28U };
    size_t report_length = 0U;

    assert(build_runtime_quality_timestamp_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_quality(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", quality_bytes, sizeof(quality_bytes), &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_timestamp(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", timestamp_bytes, sizeof(timestamp_bytes), &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);

    assert(unitlab_mms_server_runtime_update_signal_boolean(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", 1, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert(server_runtime.pending_report_member_index == 0U);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$q", strlen("IED1LD0/PGGIO1$ST$Ind1$q")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$t", strlen("IED1LD0/PGGIO1$ST$Ind1$t")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x83\x01\xFF", 3U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x7A\xBC", 4U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x91\x08\x21\x22\x23\x24\x25\x26\x27\x28", 10U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x05\xE0", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_update_signal_value_changes_read_value(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t response_bytes[1024U];
    size_t response_length = 0U;

    assert(build_runtime_value_store_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_int32(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", 42, &diagnostic));

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 61U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "IED1LD0.PGGIO1.ST.Ind1.stVal");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "stVal");
    server_runtime.pending_request.read_object_reference_count = 1U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.PGGIO1.ST.Ind1.stVal");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "stVal");

    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x85\x01\x2A", 3U) == 1);

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 63U, 8U, 1000U, 110U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "IED1LD0.PGGIO1.ST");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "ST");
    server_runtime.pending_request.read_object_reference_count = 1U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.PGGIO1.ST");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "ST");
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x85\x01\x2A", 3U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_signal_update_aliases_share_stored_dataset_value(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t first_value = 0x11U;
    uint8_t second_value = 0x22U;

    assert(build_runtime_value_store_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0.PGGIO1.ST.Ind1.stVal", &first_value, 1U, &diagnostic));
    assert(server_runtime.signal_value_count == 3U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert(server_runtime.pending_report_member_index == 1U);
    assert((server_runtime.pending_report_member_mask & 0x02U) == 0x02U);
    assert(contains_bytes(server_runtime.pending_report_value, server_runtime.pending_report_value_length, (const uint8_t*)"\x85\x01\x11", 3U) == 1);

    server_runtime_clear_pending_reports(&server_runtime);
    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &second_value, 1U, &diagnostic));
    assert(server_runtime.signal_value_count == 3U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert(server_runtime.pending_report_member_index == 1U);
    assert((server_runtime.pending_report_member_mask & 0x02U) == 0x02U);
    assert(contains_bytes(server_runtime.pending_report_value, server_runtime.pending_report_value_length, (const uint8_t*)"\x85\x01\x22", 3U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_update_dataset_member_queues_report_when_enabled(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t report_bytes[2048U];
    uint8_t value_byte = 0x01U;
    size_t report_length = 0U;

    assert(build_runtime_value_store_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &value_byte, 1U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert(server_runtime.pending_report_member_index == 1U);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert_information_report_wire_envelope(report_bytes, report_length);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x85\x01\x01", 3U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x06\x40", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_multiple_dataset_updates_share_one_data_change_report(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t report_bytes[4096U];
    uint8_t first_value = 0x03U;
    uint8_t second_value = 0x04U;
    size_t report_length = 0U;

    assert(build_runtime_value_store_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/XCBR1$ST$Pos$stVal", &first_value, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &second_value, 1U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert((server_runtime.pending_report_member_mask & 0x03U) == 0x03U);

    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert_information_report_wire_envelope(report_bytes, report_length);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/XCBR1$ST$Pos$stVal", strlen("IED1LD0/XCBR1$ST$Pos$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x85\x01\x03", 3U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x85\x01\x04", 3U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x06\xC0", 4U) == 1);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);
    assert(server_runtime.pending_report_member_mask == 0U);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_same_value_update_uses_data_update_trigger(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t report_bytes[4096U];
    uint8_t same_value = 0x00U;
    size_t report_length = 0U;

    assert(build_runtime_value_store_plan(&plan) == 1);
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI;
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &same_value, 1U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);
    assert(server_runtime.pending_gi_report == 0U);

    unitlab_mms_server_runtime_stop(&server_runtime, &diagnostic);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_DATA_UPDATE | UNITLAB_IED_MODEL_TRG_OPT_GI;
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &same_value, 1U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_UPDATE);
    assert(server_runtime.pending_report_member_index == 1U);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x85\x01\x00", 3U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x02\x20", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_data_change_trigger_disabled_does_not_queue_report(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x09U;
    size_t response_length = 0U;

    assert(build_runtime_value_store_plan(&plan) == 1);
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_GI;
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &value_byte, 1U, &diagnostic));
    assert(server_runtime.pending_gi_report == 0U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 65U, 7U, 1000U, 100U, &diagnostic) == 1);
    server_runtime.pending_request.read_object_reference_count = 1U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.PGGIO1.ST.Ind1.stVal");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "stVal");

    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x85\x01\x09", 3U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_update_non_report_dataset_signal_does_not_queue_report(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x07U;
    size_t response_length = 0U;

    assert(build_runtime_value_store_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/GGIO1$MX$AnIn1$mag$f", &value_byte, 1U, &diagnostic));
    assert(server_runtime.pending_gi_report == 0U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 62U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "IED1LD0.GGIO1.MX.AnIn1.mag.f");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "f");
    server_runtime.pending_request.read_object_reference_count = 1U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.GGIO1.MX.AnIn1.mag.f");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "f");

    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x85\x01\x07", 3U) == 1);

    unitlab_free_ied_model_plan(&plan);
}



static void test_server_runtime_dataset_write_queues_data_change_report(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t report_bytes[2048U];
    uint8_t value_byte = 0x01U;
    UnitLabMmsBerElement data_element;
    UnitLabMmsAssociationFrame report_frame;
    UnitLabMmsPdu report_pdu;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t report_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "XCBR1$ST$Pos$stVal", &data_element, 44U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.pending_gi_report == 1U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert(server_runtime.pending_report_member_index == 0U);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(server_runtime.pending_gi_report == 0U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);

    unitlab_mms_association_frame_init(&report_frame);
    assert(unitlab_mms_association_frame_decode(&report_frame, report_bytes, report_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_length);
    unitlab_mms_pdu_init(&report_pdu);
    assert(unitlab_mms_pdu_decode(&report_pdu, report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, &consumed_length, &diagnostic));
    assert(report_pdu.kind == UNITLAB_MMS_PDU_UNCONFIRMED);
    assert(report_pdu.service_kind == UNITLAB_MMS_SERVICE_INFORMATION_REPORT);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1LD0/XCBR1$ST$Pos$stVal", strlen("IED1LD0/XCBR1$ST$Pos$stVal")) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1IED1LD0", strlen("IED1IED1LD0")) == 0);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"PGGIO1$ST$Ind1$stVal", strlen("PGGIO1$ST$Ind1$stVal")) == 0);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x84\x02\x06\x80", 4U) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x85\x01\x01", 3U) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x84\x02\x02\x80", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_dataset_write_second_member_marks_second_inclusion_bit(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t report_bytes[2048U];
    uint8_t value_byte = 0x01U;
    UnitLabMmsBerElement data_element;
    UnitLabMmsAssociationFrame report_frame;
    UnitLabMmsPdu report_pdu;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t report_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "PGGIO1$ST$Ind1$stVal", &data_element, 46U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.pending_gi_report == 1U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE);
    assert(server_runtime.pending_report_member_index == 1U);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));

    unitlab_mms_association_frame_init(&report_frame);
    assert(unitlab_mms_association_frame_decode(&report_frame, report_bytes, report_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_length);
    unitlab_mms_pdu_init(&report_pdu);
    assert(unitlab_mms_pdu_decode(&report_pdu, report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, &consumed_length, &diagnostic));
    assert(report_pdu.kind == UNITLAB_MMS_PDU_UNCONFIRMED);
    assert(report_pdu.service_kind == UNITLAB_MMS_SERVICE_INFORMATION_REPORT);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"PGGIO1$ST$Ind1$stVal", strlen("PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"IED1IED1LD0", strlen("IED1IED1LD0")) == 0);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"XCBR1$ST$Pos$stVal", strlen("XCBR1$ST$Pos$stVal")) == 0);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x84\x02\x06\x40", 4U) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x85\x01\x01", 3U) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"\x84\x02\x02\x80", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}



static void test_server_runtime_report_option_fields_use_model_masks(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t response_bytes[2048U];
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    plan.reports[0].optional_fields_mask = UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM | UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE;
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 49U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "IED1LD0.LLN0.BR.brcbEvents");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "brcbEvents");
    server_runtime.pending_request.read_object_reference_count = 1U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.LLN0.BR.brcbEvents");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "brcbEvents");

    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x03\x06\x44\x00", 5U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x02\x02\x44", 4U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x03\x06\x7F\x80", 5U) == 0);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x02\x02\x64", 4U) == 0);

    unitlab_free_ied_model_plan(&plan);
}


static void test_server_runtime_report_option_scalar_reads_use_model_masks(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t response_bytes[1024U];
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    plan.reports[0].optional_fields_mask = UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM
        | UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION
        | UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW
        | UNITLAB_IED_MODEL_RPT_OPT_CONF_REV;
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED
        | UNITLAB_IED_MODEL_TRG_OPT_DATA_UPDATE
        | UNITLAB_IED_MODEL_TRG_OPT_GI;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 50U, 7U, 1000U, 100U, &diagnostic) == 1);
    server_runtime.pending_request.read_object_reference_count = 2U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.LLN0.BR.brcbEvents.OptFlds");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "OptFlds");
    snprintf(server_runtime.pending_request.read_object_references[1], sizeof(server_runtime.pending_request.read_object_references[1]), "%s", "IED1LD0.LLN0.BR.brcbEvents.TrgOps");
    snprintf(server_runtime.pending_request.read_attribute_references[1], sizeof(server_runtime.pending_request.read_attribute_references[1]), "%s", "TrgOps");

    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x03\x06\x52\x80", 5U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x02\x02\x54", 4U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x03\x06\x7F\x80", 5U) == 0);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x02\x02\x64", 4U) == 0);

    unitlab_free_ied_model_plan(&plan);
}


static void test_server_runtime_information_report_omits_disabled_optional_fields(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t report_bytes[2048U];
    uint8_t value_byte = 0x01U;
    size_t report_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    plan.reports[0].optional_fields_mask = UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM | UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_queue_data_change_report_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &value_byte, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));

    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x03\x06\x44\x00", 5U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x86\x01\x00", 3U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/LLN0$dsEvents", strlen("IED1LD0/LLN0$dsEvents")) == 0);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x8C\x06", 2U) == 0);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x89\x08", 2U) == 0);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x86\x01\x07", 3U) == 0);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x02\x80", 4U) == 0);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_data_change_reports_advance_sequence_and_entry_time(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t first_report_bytes[2048U];
    uint8_t second_report_bytes[2048U];
    uint8_t first_value_byte = 0x01U;
    uint8_t second_value_byte = 0x02U;
    uint8_t first_entry_id[8U];
    uint8_t first_time_of_entry[6U];
    UnitLabMmsBerElement data_element;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t first_report_length = 0U;
    size_t second_report_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &first_value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "PGGIO1$ST$Ind1$stVal", &data_element, 47U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, first_report_bytes, sizeof(first_report_bytes), &first_report_length, &diagnostic));
    assert(server_runtime.brcb_sq_num == 1U);
    memcpy(first_entry_id, server_runtime.brcb_entry_id, sizeof(first_entry_id));
    memcpy(first_time_of_entry, server_runtime.brcb_time_of_entry, sizeof(first_time_of_entry));
    assert(contains_bytes(first_report_bytes, first_report_length, (const uint8_t*)"\x86\x01\x00", 3U) == 1);
    assert(contains_bytes(first_report_bytes, first_report_length, (const uint8_t*)"\x86\x01\x07", 3U) == 1);
    assert(contains_bytes(first_report_bytes, first_report_length, (const uint8_t*)"\x84\x02\x06\x40", 4U) == 1);
    assert(contains_bytes(first_report_bytes, first_report_length, (const uint8_t*)"\x84\x02\x02\x80", 4U) == 1);

    assert(unitlab_mms_server_runtime_queue_data_change_report_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &second_value_byte, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, second_report_bytes, sizeof(second_report_bytes), &second_report_length, &diagnostic));
    assert(server_runtime.brcb_sq_num == 2U);
    assert(memcmp(first_entry_id, server_runtime.brcb_entry_id, sizeof(first_entry_id)) != 0);
    assert(memcmp(first_time_of_entry, server_runtime.brcb_time_of_entry, sizeof(first_time_of_entry)) != 0);
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"\x86\x01\x01", 3U) == 1);
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"\x85\x01\x02", 3U) == 1);
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"\x86\x01\x07", 3U) == 1);
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"\x84\x02\x06\x40", 4U) == 1);
    assert(contains_bytes(second_report_bytes, second_report_length, (const uint8_t*)"\x84\x02\x02\x80", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_information_report_buffer_overflow_false_by_default(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t report_bytes[4096U];
    size_t report_length = 0U;
    UnitLabIedModelPlan plan;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_int32(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", 1, &diagnostic));
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));

    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x83\x01\x00", 3U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x83\x01\x01", 3U) == 0);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_information_report_buffer_overflow_reports_and_clears(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t report_bytes[4096U];
    size_t report_length = 0U;
    UnitLabIedModelPlan plan;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(unitlab_mms_server_runtime_update_signal_int32(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", 1, &diagnostic));
    server_runtime.brcb_buffer_overflow = 1U;
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));

    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x83\x01\x01", 3U) == 1);
    assert(server_runtime.brcb_buffer_overflow == 0U);
    assert(server_runtime.reports_sent == 1U);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_pending_report_queue_overflow_sets_bufovfl(void)
{
    UnitLabMmsServerRuntime server_runtime;
    uint8_t encoded_value[3U] = { 0x85U, 0x01U, 0x01U };

    unitlab_mms_server_runtime_init(&server_runtime);
    server_runtime.pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE;
    server_runtime.pending_report_queue_count = UNITLAB_MMS_SERVER_RUNTIME_MAX_PENDING_REPORTS;

    assert(server_runtime_queue_pending_report_event(&server_runtime, UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE, 1U, encoded_value, sizeof(encoded_value)) == 0);
    assert(server_runtime.brcb_buffer_overflow == 1U);
    assert(server_runtime.report_events_dropped == 1U);
}

static void test_server_runtime_full_report_queue_coalesces_before_drop_newest(void)
{
    UnitLabMmsServerRuntime server_runtime;
    uint8_t first_value[3U] = { 0x85U, 0x01U, 0x01U };
    uint8_t second_value[3U] = { 0x85U, 0x01U, 0x02U };

    unitlab_mms_server_runtime_init(&server_runtime);
    server_runtime.pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE;
    server_runtime.pending_report_queue_count = UNITLAB_MMS_SERVER_RUNTIME_MAX_PENDING_REPORTS;
    for (size_t index = 0U; index < server_runtime.pending_report_queue_count; index++) {
        server_runtime.pending_report_queue[index].kind = UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_UPDATE;
        server_runtime.pending_report_queue[index].member_index = 0U;
        server_runtime.pending_report_queue[index].member_mask = 0x01U;
        memcpy(server_runtime.pending_report_queue[index].values[0], first_value, sizeof(first_value));
        server_runtime.pending_report_queue[index].value_lengths[0] = sizeof(first_value);
    }

    assert(server_runtime_queue_pending_report_event(&server_runtime, UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_UPDATE, 1U, second_value, sizeof(second_value)) == 1);
    assert(server_runtime.pending_report_queue_count == UNITLAB_MMS_SERVER_RUNTIME_MAX_PENDING_REPORTS);
    assert(server_runtime.pending_report_queue[0].member_index == 1U);
    assert((server_runtime.pending_report_queue[0].member_mask & 0x02U) == 0x02U);
    assert(memcmp(server_runtime.pending_report_queue[0].values[1], second_value, sizeof(second_value)) == 0);
    assert(server_runtime.report_events_queued == 1U);
    assert(server_runtime.report_events_coalesced == 1U);
    assert(server_runtime.report_events_dropped == 0U);
    assert(server_runtime.brcb_buffer_overflow == 0U);

    assert(server_runtime_queue_pending_report_event(&server_runtime, UNITLAB_MMS_SERVER_PENDING_REPORT_QUALITY_CHANGE, 1U, second_value, sizeof(second_value)) == 0);
    assert(server_runtime.pending_report_queue_count == UNITLAB_MMS_SERVER_RUNTIME_MAX_PENDING_REPORTS);
    assert(server_runtime.report_events_queued == 1U);
    assert(server_runtime.report_events_coalesced == 1U);
    assert(server_runtime.report_events_dropped == 1U);
    assert(server_runtime.brcb_buffer_overflow == 1U);
}

static void test_server_runtime_dataset_write_without_rptena_does_not_queue_report(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x01U;
    UnitLabMmsBerElement data_element;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "XCBR1$ST$Pos$stVal", &data_element, 45U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.pending_gi_report == 0U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_integrity_poll_queues_full_dataset_report(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t report_bytes[4096U];
    size_t report_length = 0U;

    assert(build_runtime_value_store_plan(&plan) == 1);
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY | UNITLAB_IED_MODEL_TRG_OPT_GI;
    plan.reports[0].integrity_period_ms_known = 1;
    plan.reports[0].integrity_period_ms = 100U;
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(server_runtime_poll_integrity_report(&server_runtime, 1000U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);
    assert(server_runtime.next_integrity_report_ms == 1100U);
    assert(server_runtime_poll_integrity_report(&server_runtime, 1099U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);
    assert(server_runtime_poll_integrity_report(&server_runtime, 1100U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_INTEGRITY);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/XCBR1$ST$Pos$stVal", strlen("IED1LD0/XCBR1$ST$Pos$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal")) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x06\xC0", 4U) == 1);
    assert(contains_bytes(report_bytes, report_length, (const uint8_t*)"\x84\x02\x02\x10", 4U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_integrity_poll_uses_active_report_control(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;

    assert(build_two_model_backed_rcb_plan(&plan) == 1);
    plan.reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY | UNITLAB_IED_MODEL_TRG_OPT_GI;
    plan.reports[0].integrity_period_ms_known = 1;
    plan.reports[0].integrity_period_ms = 100U;
    plan.reports[1].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI;
    plan.reports[1].integrity_period_ms_known = 1;
    plan.reports[1].integrity_period_ms = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(server_runtime_select_report_control_by_reference(&server_runtime, "System.LLN0.BR.brcbB.RptEna") == 1);
    assert(server_runtime.active_report_index == 1U);
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    assert(server_runtime_poll_integrity_report(&server_runtime, 1000U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);
    assert(server_runtime.pending_gi_report == 0U);
    assert(server_runtime.next_integrity_report_ms == 0U);
    assert(server_runtime_poll_integrity_report(&server_runtime, 1100U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);
    assert(server_runtime.pending_gi_report == 0U);
    assert(server_runtime.next_integrity_report_ms == 0U);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_information_report_requires_model_dataset(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t report_bytes[2048U];
    size_t report_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;
    server_runtime.pending_gi_report = 1U;
    server_runtime.pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_GI;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic) == 0);
    assert(report_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
}

static void test_server_runtime_gi_requires_enabled_rptena(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x01U;
    UnitLabMmsBerElement data_element;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$GI", &data_element, 23U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x17U, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);
    assert(server_runtime.pending_gi_report == 0U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
}

static void test_server_runtime_disabled_rcb_accepts_option_and_trigger_writes(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x01U;
    const uint8_t opt_flds_value[] = { 0x06U, 0x40U, 0x00U };
    const uint8_t trg_ops_value[] = { 0x02U, 0x0CU };
    UnitLabMmsBerElement data_element;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 4U;
    data_element.value_bytes = opt_flds_value;
    data_element.value_length = sizeof(opt_flds_value);
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$OptFlds", &data_element, 51U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x33U, 0xA5U, 0x02U, 0x81U, 0x00U }, 7U) == 1);
    assert(server_runtime.brcb_optional_fields_mask_known == 1U);
    assert(server_runtime.brcb_optional_fields_mask == UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 4U;
    data_element.value_bytes = trg_ops_value;
    data_element.value_length = sizeof(trg_ops_value);
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$TrgOps", &data_element, 52U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x34U, 0xA5U, 0x02U, 0x81U, 0x00U }, 7U) == 1);
    assert(server_runtime.brcb_trigger_options_mask_known == 1U);
    assert(server_runtime.brcb_trigger_options_mask == (UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY | UNITLAB_IED_MODEL_TRG_OPT_GI));
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 53U, 7U, 1000U, 100U, &diagnostic) == 1);
    server_runtime.pending_request.read_object_reference_count = 2U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.LLN0.BR.brcbEvents.OptFlds");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "OptFlds");
    snprintf(server_runtime.pending_request.read_object_references[1], sizeof(server_runtime.pending_request.read_object_references[1]), "%s", "IED1LD0.LLN0.BR.brcbEvents.TrgOps");
    snprintf(server_runtime.pending_request.read_attribute_references[1], sizeof(server_runtime.pending_request.read_attribute_references[1]), "%s", "TrgOps");
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x03\x06\x40\x00", 5U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"\x84\x02\x02\x0C", 4U) == 1);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;
    assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &value_byte, 1U, &diagnostic));
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_enabled_rcb_rejects_static_config_write(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    const char dataset_reference[] = "IED1LD0/LLN0$dsEvents";
    UnitLabMmsBerElement data_element;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 10U;
    data_element.value_bytes = (const uint8_t*)dataset_reference;
    data_element.value_length = strlen(dataset_reference);
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$DatSet", &data_element, 31U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x1FU, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_enabled_rcb_rejects_static_field_matrix(void)
{
    static const uint8_t conf_rev_value[] = { 0x07U };
    static const uint8_t opt_flds_value[] = { 0x06U, 0x11U, 0x00U };
    static const uint8_t buf_tm_value[] = { 0x64U };
    static const uint8_t trg_ops_value[] = { 0x02U, 0x44U };
    static const uint8_t intg_pd_value[] = { 0x03U, 0xE8U };
    static const struct {
        const char* field_name;
        uint32_t tag_number;
        const uint8_t* value_bytes;
        size_t value_length;
        uint32_t invoke_id;
    } cases[] = {
        { "ConfRev", 6U, conf_rev_value, sizeof(conf_rev_value), 40U },
        { "OptFlds", 4U, opt_flds_value, sizeof(opt_flds_value), 41U },
        { "BufTm", 6U, buf_tm_value, sizeof(buf_tm_value), 42U },
        { "TrgOps", 4U, trg_ops_value, sizeof(trg_ops_value), 43U },
        { "IntgPd", 6U, intg_pd_value, sizeof(intg_pd_value), 44U },
    };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    char item_id[96U];
    UnitLabMmsBerElement data_element;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++) {
        snprintf(item_id, sizeof(item_id), "LLN0$BR$brcbEvents$%s", cases[index].field_name);
        unitlab_mms_ber_element_init(&data_element);
        data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        data_element.tag.constructed = 0;
        data_element.tag.tag_number = cases[index].tag_number;
        data_element.value_bytes = cases[index].value_bytes;
        data_element.value_length = cases[index].value_length;
        assert(unitlab_mms_build_write_request_frame("IED1LD0", item_id, &data_element, cases[index].invoke_id, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
        unitlab_mms_operation_result_init(&operation_result);
        assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
        assert(operation_result.ok == 1);
        assert(consumed_length == request_length);
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
        assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, (uint8_t)cases[index].invoke_id, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);
        assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));
    }

    assert(server_runtime.brcb_rpt_ena == 1U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_enabled_rcb_rejects_identity_and_owner_writes(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[768U];
    uint8_t request_bytes[768U];
    uint8_t response_bytes[1024U];
    const char rpt_id[] = "IED1LD0/LLN0$BR$brcbEvents";
    const char owner[] = "other-client";
    UnitLabMmsBerElement data_element;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;
    snprintf(server_runtime.brcb_owner, sizeof(server_runtime.brcb_owner), "%s", "local-client");

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 10U;
    data_element.value_bytes = (const uint8_t*)rpt_id;
    data_element.value_length = strlen(rpt_id);
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$RptID", &data_element, 32U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x20U, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 10U;
    data_element.value_bytes = (const uint8_t*)owner;
    data_element.value_length = strlen(owner);
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$Owner", &data_element, 33U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x21U, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);
    assert(strcmp(server_runtime.brcb_owner, "local-client") == 0);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_enabled_rcb_rejects_resvtms_release_write(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x00U;
    UnitLabMmsBerElement data_element;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_resv_tms = 30U;
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$ResvTms", &data_element, 33U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x21U, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);
    assert(server_runtime.brcb_resv_tms == 30U);
    assert(server_runtime.brcb_rpt_ena == 1U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_unsupported_signal_write_returns_failure_result(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x01U;
    UnitLabMmsBerElement data_element;
    UnitLabIedModelPlan plan;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    assert(build_data_change_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 5U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "UNKNOWN1$ST$X$stVal", &data_element, 32U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x20U, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);

    unitlab_free_ied_model_plan(&plan);
}


static void test_server_runtime_rejects_invalid_rcb_boolean_writes(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t invalid_boolean = 0x02U;
    UnitLabMmsBerElement data_element;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &invalid_boolean;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$RptEna", &data_element, 34U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x22U, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);
    assert(server_runtime.brcb_rpt_ena == 0U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$GI", &data_element, 35U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x23U, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);
    assert(server_runtime.pending_gi_report == 0U);
    assert(server_runtime.pending_report_kind == UNITLAB_MMS_SERVER_PENDING_REPORT_NONE);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    server_runtime.brcb_rpt_ena = 0U;
    server_runtime.brcb_sq_num = 5U;
    server_runtime.pending_gi_report = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$PurgeBuf", &data_element, 36U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x24U, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);
    assert(server_runtime.brcb_sq_num == 5U);
    assert(server_runtime.pending_gi_report == 1U);
}

static void test_server_runtime_rptena_false_clears_pending_gi(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x00U;
    UnitLabMmsBerElement data_element;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;
    server_runtime.pending_gi_report = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$RptEna", &data_element, 24U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.brcb_rpt_ena == 0U);
    assert(server_runtime.pending_gi_report == 0U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
}

static void test_server_runtime_purgebuf_write_resets_brcb_runtime_state(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t value_byte = 0x01U;
    UnitLabMmsBerElement data_element;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    server_runtime.brcb_sq_num = 12U;
    server_runtime.brcb_entry_id_counter = 12U;
    memset(server_runtime.brcb_entry_id, 0xAA, sizeof(server_runtime.brcb_entry_id));
    memset(server_runtime.brcb_time_of_entry, 0xBB, sizeof(server_runtime.brcb_time_of_entry));
    server_runtime.pending_gi_report = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$PurgeBuf", &data_element, 25U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(server_runtime.brcb_sq_num == 0U);
    assert(server_runtime.brcb_entry_id_counter == 0U);
    assert(server_runtime.pending_gi_report == 0U);
    assert(contains_bytes(server_runtime.brcb_entry_id, sizeof(server_runtime.brcb_entry_id), (const uint8_t*)"\xAA", 1U) == 0);
    assert(contains_bytes(server_runtime.brcb_time_of_entry, sizeof(server_runtime.brcb_time_of_entry), (const uint8_t*)"\xBB", 1U) == 0);
}

static int build_get_name_list_request_association_bytes(const uint8_t* request_body_bytes, size_t request_body_length, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);


static int build_initiate_request_association_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsAssociationFrame fixture;
    UnitLabMmsAcseApdu acse_apdu;
    uint8_t acse_payload[3U] = { 0x80U, 0x01U, 0x01U };
    uint8_t acse_encoded[16U];
    size_t acse_length = 0U;
    size_t frame_length = 0U;

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    acse_apdu.apdu_bytes = acse_payload;
    acse_apdu.apdu_length = sizeof(acse_payload);

    if (!unitlab_mms_acse_encode(&acse_apdu, acse_encoded, sizeof(acse_encoded), &acse_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_association_frame_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    fixture.presentation.payload_bytes = acse_encoded;
    fixture.presentation.payload_length = acse_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;

    if (!unitlab_mms_association_frame_encode(&fixture, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }
    *encoded_length = frame_length;
    return 1;
}

static int build_model_read_request_association_bytes(const char* raw_object_reference, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    char domain_id[128U];
    char item_id[128U];
    const char* separator = NULL;
    size_t domain_length = 0U;
    uint8_t scratch[256U];

    if (raw_object_reference == NULL || buffer == NULL || encoded_length == NULL) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return 0;
    }

    separator = strchr(raw_object_reference, '$');
    if (separator == NULL || separator == raw_object_reference) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return 0;
    }

    domain_length = (size_t)(separator - raw_object_reference);
    if (domain_length >= sizeof(domain_id)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
        }
        return 0;
    }
    memcpy(domain_id, raw_object_reference, domain_length);
    domain_id[domain_length] = '\0';

    if (strlen(separator + 1U) >= sizeof(item_id)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
        }
        return 0;
    }
    snprintf(item_id, sizeof(item_id), "%s", separator + 1U);

    return unitlab_mms_build_read_request_frame(domain_id, item_id, invoke_id, scratch, sizeof(scratch), buffer, buffer_length, encoded_length, diagnostic);
}

static void test_server_runtime_apply_association_request_bytes_accepts_acse_aarq(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    assert(build_initiate_request_association_bytes(wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.transport.request_bytes == wire_bytes);
    assert(server_runtime.transport.request_length == wire_length);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(server_runtime.session.active_invoke_id == 1U);
    assert(server_runtime.last_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION);
}

static void test_server_runtime_apply_association_request_bytes_accepts_captured_iedscout_aarq(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    static const uint8_t wire_bytes[] = { 0x03U, 0x00U, 0x00U, 0xD3U, 0x02U, 0xF0U, 0x80U, 0x0DU, 0xCAU, 0x05U, 0x06U, 0x13U, 0x01U, 0x00U, 0x16U, 0x01U, 0x02U, 0x14U, 0x02U, 0x00U, 0x02U, 0x33U, 0x02U, 0x00U, 0x01U, 0x34U, 0x02U, 0x00U, 0x01U, 0xC1U, 0xB4U, 0x31U, 0x81U, 0xB1U, 0xA0U, 0x03U, 0x80U, 0x01U, 0x01U, 0xA2U, 0x81U, 0xA9U, 0x81U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U, 0x82U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U, 0xA4U, 0x23U, 0x30U, 0x0FU, 0x02U, 0x01U, 0x01U, 0x06U, 0x04U, 0x52U, 0x01U, 0x00U, 0x01U, 0x30U, 0x04U, 0x06U, 0x02U, 0x51U, 0x01U, 0x30U, 0x10U, 0x02U, 0x01U, 0x03U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x22U, 0x02U, 0x01U, 0x30U, 0x04U, 0x06U, 0x02U, 0x51U, 0x01U, 0x61U, 0x76U, 0x30U, 0x74U, 0x02U, 0x01U, 0x01U, 0xA0U, 0x6FU, 0x60U, 0x6DU, 0xA1U, 0x07U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U, 0xA2U, 0x07U, 0x06U, 0x05U, 0x29U, 0x01U, 0x87U, 0x67U, 0x01U, 0xA3U, 0x03U, 0x02U, 0x01U, 0x0CU, 0xA4U, 0x03U, 0x02U, 0x01U, 0x00U, 0xA5U, 0x03U, 0x02U, 0x01U, 0x00U, 0xA6U, 0x06U, 0x06U, 0x04U, 0x29U, 0x01U, 0x87U, 0x67U, 0xA7U, 0x03U, 0x02U, 0x01U, 0x0CU, 0xA8U, 0x03U, 0x02U, 0x01U, 0x00U, 0xA9U, 0x03U, 0x02U, 0x01U, 0x00U, 0xBEU, 0x33U, 0x28U, 0x31U, 0x06U, 0x02U, 0x51U, 0x01U, 0x02U, 0x01U, 0x03U, 0xA0U, 0x28U, 0xA8U, 0x26U, 0x80U, 0x03U, 0x00U, 0xFDU, 0xE8U, 0x81U, 0x01U, 0x0AU, 0x82U, 0x01U, 0x0AU, 0x83U, 0x01U, 0x05U, 0xA4U, 0x16U, 0x80U, 0x01U, 0x01U, 0x81U, 0x03U, 0x05U, 0xF1U, 0x00U, 0x82U, 0x0CU, 0x03U, 0xEEU, 0x1CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xEDU, 0x18U };
    size_t consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, wire_bytes, sizeof(wire_bytes), &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(wire_bytes));
    assert(server_runtime.transport.request_bytes == wire_bytes);
    assert(server_runtime.transport.request_length == sizeof(wire_bytes));
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(server_runtime.session.active_invoke_id == 1U);
    assert(server_runtime.last_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION);
}

static void test_server_runtime_build_association_response_matches_reference_capture(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t request_bytes[256];
    uint8_t response_bytes[256];
    size_t request_length = 0U;
    size_t response_length = 0U;
    size_t consumed_length = 0U;
    static const uint8_t expected_response[] = { 0x03U, 0x00U, 0x00U, 0x8FU, 0x02U, 0xF0U, 0x80U, 0x0EU, 0x86U, 0x05U, 0x06U, 0x13U, 0x01U, 0x00U, 0x16U, 0x01U, 0x02U, 0x14U, 0x02U, 0x00U, 0x02U, 0x34U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x74U, 0x31U, 0x72U, 0xA0U, 0x03U, 0x80U, 0x01U, 0x01U, 0xA2U, 0x6BU, 0x83U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U, 0xA5U, 0x12U, 0x30U, 0x07U, 0x80U, 0x01U, 0x00U, 0x81U, 0x02U, 0x51U, 0x01U, 0x30U, 0x07U, 0x80U, 0x01U, 0x00U, 0x81U, 0x02U, 0x51U, 0x01U, 0x61U, 0x4FU, 0x30U, 0x4DU, 0x02U, 0x01U, 0x01U, 0xA0U, 0x48U, 0x61U, 0x46U, 0xA1U, 0x07U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U, 0xA2U, 0x03U, 0x02U, 0x01U, 0x00U, 0xA3U, 0x05U, 0xA1U, 0x03U, 0x02U, 0x01U, 0x00U, 0xBEU, 0x2FU, 0x28U, 0x2DU, 0x02U, 0x01U, 0x03U, 0xA0U, 0x28U, 0xA9U, 0x26U, 0x80U, 0x03U, 0x00U, 0xFDU, 0xE8U, 0x81U, 0x01U, 0x05U, 0x82U, 0x01U, 0x05U, 0x83U, 0x01U, 0x05U, 0xA4U, 0x16U, 0x80U, 0x01U, 0x01U, 0x81U, 0x03U, 0x05U, 0xF1U, 0x00U, 0x82U, 0x0CU, 0x03U, 0xEEU, 0x1CU, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x40U, 0xEDU, 0x18U };

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    assert(build_initiate_request_association_bytes(request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(consumed_length == request_length);

    assert(unitlab_mms_build_association_response_frame_with_profile(&server_runtime.initiate_response_profile, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(response_length == sizeof(expected_response));
    assert(memcmp(response_bytes, expected_response, sizeof(expected_response)) == 0);
}

static void test_server_runtime_apply_association_then_confirmed_request_keeps_session_associated(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t association_bytes[256U];
    uint8_t request_bytes[256U];
    size_t association_length = 0U;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    const uint8_t request_payload[] = {
        0x30U, 0x0CU,
        0xA0U, 0x03U, 0x02U, 0x01U, 0x02U,
        0xA1U, 0x05U, 0x81U, 0x03U, 'L', 'D', '0'
    };

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    assert(build_initiate_request_association_bytes(association_bytes, sizeof(association_bytes), &association_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, association_bytes, association_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == association_length);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(unitlab_mms_session_complete_association(&server_runtime.session, server_runtime.session.active_invoke_id, &diagnostic));
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATED);

    assert(build_get_name_list_request_association_bytes(request_payload, sizeof(request_payload), 61U, request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == request_length);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATED);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.invoke_id == 61U);
}

static void test_server_runtime_apply_association_request_bytes_rejects_non_initiate_request(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    assert(build_information_report_association_bytes(wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result) == 0);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
}


static void test_server_runtime_apply_model_plan_sets_active_model(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabIedModelPlan plan;

    memset(&server_runtime, 0, sizeof(server_runtime));
    memset(&plan, 0, sizeof(plan));

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(server_runtime.model_plan == &plan);
    assert(server_runtime.has_read_response_value == 0);
    assert(server_runtime.read_response_value_length == 0U);
}

static void test_server_runtime_init_captures_default_snapshot(void)
{
    UnitLabMmsServerRuntime server_runtime;

    unitlab_mms_server_runtime_init(&server_runtime);

    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_IDLE);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(server_runtime.snapshot.session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
    assert(server_runtime.snapshot.report_control_state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
}

static void test_server_runtime_prepare_start_stop(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_PREPARED);
    assert(server_runtime.config.port == 102);

    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_RUNNING);

    assert(unitlab_mms_server_runtime_stop(&server_runtime, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_STOPPED);
}

static void test_server_runtime_apply_wire_pdu_requires_running_state(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsPdu wire_pdu = make_information_report_pdu();
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };

    unitlab_mms_server_runtime_init(&server_runtime);
    unitlab_mms_operation_result_init(&operation_result);

    assert(!unitlab_mms_server_runtime_apply_wire_pdu(&server_runtime, &wire_pdu, &operation_result));
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BAD_STATE);

    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED);
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED);
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING);

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_wire_pdu(&server_runtime, &wire_pdu, &operation_result));
    assert(operation_result.ok == 1);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_RUNNING);
}

static void test_wire_builder_builds_confirmed_response_frame_roundtrips(void)
{
    UnitLabMmsPdu response_pdu;
    UnitLabMmsDiagnostic diagnostic;
    uint8_t response_bytes[256];
    uint8_t response_payload[6] = { 0x02U, 0x01U, 0x29U, 0xA4U, 0x01U, 0xAAU };
    size_t encoded_length = 0U;

    unitlab_mms_pdu_init(&response_pdu);
    response_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    response_pdu.has_invoke_id = 1;
    response_pdu.invoke_id = 41U;
    response_pdu.has_service = 1;
    response_pdu.service_kind = UNITLAB_MMS_SERVICE_READ;
    response_pdu.pdu_bytes = response_payload;
    response_pdu.pdu_length = sizeof(response_payload);

    uint8_t scratch[256];
    assert(unitlab_mms_build_confirmed_response_frame(&response_pdu, scratch, sizeof(scratch), response_bytes, sizeof(response_bytes), &encoded_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(encoded_length > 0U);

    {
        UnitLabMmsTransportFrame transport_frame;
        UnitLabMmsSessionSpdu session_spdu;
        UnitLabMmsPresentationApdu presentation_apdu;
        size_t transport_consumed_length = 0U;
        size_t session_consumed_length = 0U;
        size_t presentation_consumed_length = 0U;

        unitlab_mms_transport_frame_init(&transport_frame);
        assert(unitlab_mms_transport_frame_decode(&transport_frame, response_bytes, encoded_length, &transport_consumed_length, &diagnostic));
        assert(transport_consumed_length == encoded_length);
        unitlab_mms_session_spdu_init(&session_spdu);
        assert(unitlab_mms_session_spdu_decode(&session_spdu, transport_frame.cotp.user_data, transport_frame.cotp.user_data_length, &session_consumed_length, &diagnostic));
        assert(session_consumed_length == transport_frame.cotp.user_data_length);
        unitlab_mms_presentation_apdu_init(&presentation_apdu);
        assert(unitlab_mms_presentation_decode(&presentation_apdu, session_spdu.raw_parameter_bytes, session_spdu.raw_parameter_length, &presentation_consumed_length, &diagnostic));
        assert(presentation_consumed_length == session_spdu.raw_parameter_length);
        assert(presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        {
            UnitLabMmsPdu decoded_response_pdu;
            size_t response_pdu_consumed_length = 0U;

            unitlab_mms_pdu_init(&decoded_response_pdu);
            assert(unitlab_mms_pdu_decode(&decoded_response_pdu, presentation_apdu.payload_bytes, presentation_apdu.payload_length, &response_pdu_consumed_length, &diagnostic));
            assert(response_pdu_consumed_length == presentation_apdu.payload_length);
            assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
            assert(decoded_response_pdu.has_invoke_id == 1);
            assert(decoded_response_pdu.invoke_id == 41U);
            assert(decoded_response_pdu.has_service == 1);
            assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
            assert(decoded_response_pdu.pdu_length == sizeof(response_payload));
            assert(memcmp(decoded_response_pdu.pdu_bytes, response_payload, sizeof(response_payload)) == 0);
        }
    }
    (void)response_pdu;
    (void)encoded_length;
}

static void test_server_runtime_build_confirmed_response_bytes_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelSignal signals[1U];
    uint8_t response_bytes[256];
    uint8_t response_payload[6] = { 0x02U, 0x01U, 0x29U, 0xA4U, 0x01U, 0xAAU };
    UnitLabMmsAssociationFrame fixture;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(signals, 0, sizeof(signals));
    strcpy(signals[0].object_reference, "XCBR1.ST.Pos.stVal");
    signals[0].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
    strcpy(signals[0].initial_value, "model-read");
    plan.signal_count = 1U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 41U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "XCBR1.ST.Pos.stVal");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "stVal");

    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &encoded_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(encoded_length > 0U);
    assert(server_runtime.transport.response_bytes == response_bytes);
    assert(server_runtime.transport.response_length == encoded_length);
    assert(server_runtime.transport.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH);

    unitlab_mms_association_frame_init(&fixture);
    assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, encoded_length, &consumed_length, &diagnostic));
    assert(consumed_length == encoded_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(fixture.presentation.payload_length > 0U);
    assert(fixture.presentation.payload_length > sizeof(response_payload));
    assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"model-read", strlen("model-read")) == 1);

    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 1234U, &diagnostic));
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_COMPLETED);

    (void)fixture;
    (void)consumed_length;
}


static void test_server_runtime_apply_incoming_bytes_roundtrips_and_consumes_exact_frame(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING);

    assert(build_information_report_association_bytes(wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.transport.request_bytes == wire_bytes);
    assert(server_runtime.transport.request_length == consumed_length);
    assert(server_runtime.transport.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_RUNNING);
}

static void test_server_runtime_apply_reference_confirmed_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelSignal signals[1U];
    uint8_t wire_bytes[256];
    uint8_t response_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(signals, 0, sizeof(signals));
    strcpy(signals[0].object_reference, "XCBR1.ST.Pos.stVal");
    signals[0].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
    strcpy(signals[0].initial_value, "model-read");
    plan.signal_count = 1U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));

    assert(build_model_read_request_association_bytes("XCBR1$ST$Pos$stVal", 3U, wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    {
        UnitLabMmsAssociationFrame request_fixture;
        UnitLabMmsPdu decoded_request_pdu;
        size_t request_consumed_length = 0U;

        unitlab_mms_association_frame_init(&request_fixture);
        assert(unitlab_mms_association_frame_decode(&request_fixture, wire_bytes, wire_length, &request_consumed_length, &diagnostic));
        assert(request_consumed_length == wire_length);
        assert(request_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        unitlab_mms_pdu_init(&decoded_request_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_request_pdu, request_fixture.presentation.payload_bytes, request_fixture.presentation.payload_length, &request_consumed_length, &diagnostic));
        assert(decoded_request_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
        assert(decoded_request_pdu.has_service == 1);
        assert(decoded_request_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
        assert(decoded_request_pdu.has_invoke_id == 1);
        assert(decoded_request_pdu.invoke_id == 3U);
    }
    unitlab_mms_operation_result_init(&operation_result);
    {
        int incoming_ok = unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result);
        if (!incoming_ok) {
            fprintf(stderr, "runtime diag: %d %s\n", operation_result.diagnostic.code, operation_result.diagnostic.message);
            }
        assert(incoming_ok);
    }
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.transport.invoke_id == 3U);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_READ);
    assert(server_runtime.pending_request.invoke_id == 3U);
    assert(strcmp(server_runtime.pending_request.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(strcmp(server_runtime.pending_request.attribute_reference, "stVal") == 0);
    assert(server_runtime.pending_request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED);

    unitlab_mms_diagnostic_clear(&diagnostic);
    if (!unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic)) {
        assert(0);
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        assert(fixture.presentation.payload_length > 0U);

        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"model-read", strlen("model-read")) == 1);
    }
}

static void test_server_runtime_apply_direct_read_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t association_bytes[256U];
    uint8_t read_wire_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x42U, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U,
        0x61U, 0x35U, 0x30U, 0x33U, 0x02U, 0x01U, 0x03U, 0xA0U, 0x2EU, 0xA0U, 0x2CU, 0x02U, 0x01U, 0x0AU,
        0xA4U, 0x27U, 0x80U, 0x01U, 0x00U, 0xA1U, 0x22U, 0xA0U, 0x20U, 0x30U, 0x1EU, 0xA0U, 0x1CU, 0xA1U, 0x1AU,
        0x1AU, 0x03U, 'L', 'D', '0',
        0x1AU, 0x13U, 'L', 'L', 'N', '0', '$', 'E', 'X', '$', 'N', 'a', 'm', 'P', 'l', 't', '$', 'l', 'd', 'N', 's'
    };
    uint8_t response_bytes[256U];
    size_t association_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(build_initiate_request_association_bytes(association_bytes, sizeof(association_bytes), &association_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, association_bytes, association_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(server_runtime.session.active_invoke_id == 1U);
    assert(unitlab_mms_session_complete_association(&server_runtime.session, server_runtime.session.active_invoke_id, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, read_wire_bytes, sizeof(read_wire_bytes), &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(read_wire_bytes));
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_READ);
    assert(server_runtime.pending_request.invoke_id == 10U);
    assert(strcmp(server_runtime.pending_request.object_reference, "LD0.LLN0.EX.NamPlt.ldNs") == 0);
    assert(strcmp(server_runtime.pending_request.attribute_reference, "ldNs") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame response_frame;
        UnitLabMmsPdu decoded_response_pdu;
        size_t response_pdu_consumed_length = 0U;

        unitlab_mms_association_frame_init(&response_frame);
        assert(unitlab_mms_association_frame_decode(&response_frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(response_frame.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        assert(response_frame.presentation.payload_length > 0U);
        assert(contains_bytes(response_frame.presentation.payload_bytes, response_frame.presentation.payload_length, (const uint8_t*)"LD0", strlen("LD0")) == 1);

        {
            static const uint8_t expected_payload_bytes[] = {
                0x02U, 0x01U, 0x0AU, 0xA4U, 0x07U, 0xA1U, 0x05U, 0x8AU, 0x03U, 'L', 'D', '0'
            };

            assert(contains_bytes(response_frame.presentation.payload_bytes, response_frame.presentation.payload_length, expected_payload_bytes, sizeof(expected_payload_bytes)) == 1);
        }

        unitlab_mms_pdu_init(&decoded_response_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_response_pdu, response_frame.presentation.payload_bytes, response_frame.presentation.payload_length, &response_pdu_consumed_length, &diagnostic));
        assert(response_pdu_consumed_length == response_frame.presentation.payload_length);
        assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response_pdu.has_invoke_id == 1);
        assert(decoded_response_pdu.invoke_id == 10U);
        assert(decoded_response_pdu.has_service == 1);
        assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
        assert(decoded_response_pdu.pdu_length > 0U);
        assert(decoded_response_pdu.pdu_bytes[0] == 0x02U);
        assert(decoded_response_pdu.pdu_bytes[3] == 0xA4U);
        assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 1234U, &diagnostic));
    }

    {
        uint8_t vendor_read_request[256U];
        uint8_t vendor_scratch[1024U];
        uint8_t vendor_response_bytes[256U];
        size_t vendor_read_request_length = 0U;
        size_t vendor_response_length = 0U;

        unitlab_mms_diagnostic_clear(&diagnostic);
        assert(unitlab_mms_build_read_request_frame("LD0", "LLN0$DC$NamPlt$vendor", 11U, vendor_scratch, sizeof(vendor_scratch), vendor_read_request, sizeof(vendor_read_request), &vendor_read_request_length, &diagnostic));
        unitlab_mms_operation_result_init(&operation_result);
        assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, vendor_read_request, vendor_read_request_length, &consumed_length, &operation_result));
        assert(operation_result.ok == 1);
        assert(consumed_length == vendor_read_request_length);
        unitlab_mms_diagnostic_clear(&diagnostic);
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, vendor_response_bytes, sizeof(vendor_response_bytes), &vendor_response_length, &diagnostic));
        assert_read_response_success_visible_string(vendor_response_bytes, vendor_response_length, 11U, "UnitLab");
        assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 1235U, &diagnostic));
    }

    {
        uint8_t namespace_response_bytes[512U];
        size_t namespace_response_length = 0U;

        assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 10U, 0U, 1000U, 0U, &diagnostic) == 1);
        snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "LD0.LLN0.EX.NamPlt.ldNs");
        snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "ldNs");
        server_runtime.pending_request.read_object_reference_count = 4U;
        snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "LD0.LLN0.EX.NamPlt.ldNs");
        snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "ldNs");
        snprintf(server_runtime.pending_request.read_object_references[1], sizeof(server_runtime.pending_request.read_object_references[1]), "%s", "LD0.LLN0.EX.NamPlt.lnNs");
        snprintf(server_runtime.pending_request.read_attribute_references[1], sizeof(server_runtime.pending_request.read_attribute_references[1]), "%s", "lnNs");
        snprintf(server_runtime.pending_request.read_object_references[2], sizeof(server_runtime.pending_request.read_object_references[2]), "%s", "LD0.LLN0.EX.NamPlt.cdcNs");
        snprintf(server_runtime.pending_request.read_attribute_references[2], sizeof(server_runtime.pending_request.read_attribute_references[2]), "%s", "cdcNs");
        snprintf(server_runtime.pending_request.read_object_references[3], sizeof(server_runtime.pending_request.read_object_references[3]), "%s", "LD0.LLN0.EX.NamPlt.dataNs");
        snprintf(server_runtime.pending_request.read_attribute_references[3], sizeof(server_runtime.pending_request.read_attribute_references[3]), "%s", "dataNs");

        unitlab_mms_diagnostic_clear(&diagnostic);
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, namespace_response_bytes, sizeof(namespace_response_bytes), &namespace_response_length, &diagnostic));
        {
            static const char* const expected_namespace_values[] = {
                "LD0",
                "IEC 61850-7-4:2007",
                "IEC 61850-7-3:2010",
                "EXT:2015",
            };

            assert_read_response_list_of_access_results_count(namespace_response_bytes, namespace_response_length, 10U, 4U, expected_namespace_values);
        }
    }


}

static void test_server_runtime_apply_iedscout_namespace_multi_read_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t association_bytes[256U];
    uint8_t read_wire_bytes[] = {
        0x03U, 0x00U, 0x00U, 0xACU, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U,
        0x61U, 0x81U, 0x9EU, 0x30U, 0x81U, 0x9BU, 0x02U, 0x01U, 0x03U, 0xA0U, 0x81U, 0x95U,
        0xA0U, 0x81U, 0x92U, 0x02U, 0x01U, 0x0AU, 0xA4U, 0x81U, 0x8CU, 0x80U, 0x01U, 0x00U,
        0xA1U, 0x81U, 0x86U, 0xA0U, 0x81U, 0x83U,
        0x30U, 0x1EU, 0xA0U, 0x1CU, 0xA1U, 0x1AU, 0x1AU, 0x03U, 'L', 'D', '0',
        0x1AU, 0x13U, 'L', 'L', 'N', '0', '$', 'E', 'X', '$', 'N', 'a', 'm', 'P', 'l', 't', '$', 'l', 'd', 'N', 's',
        0x30U, 0x1EU, 0xA0U, 0x1CU, 0xA1U, 0x1AU, 0x1AU, 0x03U, 'L', 'D', '0',
        0x1AU, 0x13U, 'L', 'L', 'N', '0', '$', 'E', 'X', '$', 'N', 'a', 'm', 'P', 'l', 't', '$', 'l', 'n', 'N', 's',
        0x30U, 0x1FU, 0xA0U, 0x1DU, 0xA1U, 0x1BU, 0x1AU, 0x03U, 'L', 'D', '0',
        0x1AU, 0x14U, 'L', 'L', 'N', '0', '$', 'E', 'X', '$', 'N', 'a', 'm', 'P', 'l', 't', '$', 'c', 'd', 'c', 'N', 's',
        0x30U, 0x20U, 0xA0U, 0x1EU, 0xA1U, 0x1CU, 0x1AU, 0x03U, 'L', 'D', '0',
        0x1AU, 0x15U, 'L', 'L', 'N', '0', '$', 'E', 'X', '$', 'N', 'a', 'm', 'P', 'l', 't', '$', 'd', 'a', 't', 'a', 'N', 's'
    };
    uint8_t response_bytes[512U];
    size_t association_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(build_initiate_request_association_bytes(association_bytes, sizeof(association_bytes), &association_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, association_bytes, association_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_session_complete_association(&server_runtime.session, server_runtime.session.active_invoke_id, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    if (!unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, read_wire_bytes, sizeof(read_wire_bytes), &consumed_length, &operation_result)) {
        fprintf(stderr, "namespace multi-read diag: %d %s\n", operation_result.diagnostic.code, operation_result.diagnostic.message);
        assert(0);
    }
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(read_wire_bytes));
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_READ);
    assert(server_runtime.pending_request.invoke_id == 10U);
    assert(server_runtime.pending_request.read_object_reference_count == 4U);
    assert(strcmp(server_runtime.pending_request.read_object_references[0], "LD0.LLN0.EX.NamPlt.ldNs") == 0);
    assert(strcmp(server_runtime.pending_request.read_object_references[1], "LD0.LLN0.EX.NamPlt.lnNs") == 0);
    assert(strcmp(server_runtime.pending_request.read_object_references[2], "LD0.LLN0.EX.NamPlt.cdcNs") == 0);
    assert(strcmp(server_runtime.pending_request.read_object_references[3], "LD0.LLN0.EX.NamPlt.dataNs") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    {
        static const char* const expected_namespace_values[] = {
            "LD0",
            "IEC 61850-7-4:2007",
            "IEC 61850-7-3:2010",
            "EXT:2015",
        };

        assert_read_response_list_of_access_results_count(response_bytes, response_length, 10U, 4U, expected_namespace_values);
    }
}

static void test_server_runtime_apply_iedscout_buffered_report_control_block_read_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelLogicalDevice logical_device;
    UnitLabIedModelPlan model_plan;
    uint8_t association_bytes[256U];
    uint8_t read_wire_bytes[256U];
    uint8_t response_bytes[1024U];
    size_t association_length = 0U;
    size_t read_wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    memset(&logical_device, 0, sizeof(logical_device));
    memset(&model_plan, 0, sizeof(model_plan));
    snprintf(logical_device.inst, sizeof(logical_device.inst), "%s", "IED1LD0");
    model_plan.logical_device_count = 1U;
    model_plan.logical_devices = &logical_device;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &model_plan) == 1);
    assert(build_initiate_request_association_bytes(association_bytes, sizeof(association_bytes), &association_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, association_bytes, association_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_session_complete_association(&server_runtime.session, server_runtime.session.active_invoke_id, &diagnostic));

    assert(build_model_read_request_association_bytes("LD0$LLN0$BR$brcbEvents", 14U, read_wire_bytes, sizeof(read_wire_bytes), &read_wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, read_wire_bytes, read_wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == read_wire_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_READ);
    assert(server_runtime.pending_request.invoke_id == 14U);
    assert(strcmp(server_runtime.pending_request.object_reference, "LD0.LLN0.BR.brcbEvents") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
    assert_read_response_success_rcb_structure(response_bytes, response_length, 14U);
}



static void test_server_runtime_apply_iedscout_buffered_report_control_block_container_read_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelLogicalDevice logical_device;
    UnitLabIedModelPlan model_plan;
    uint8_t association_bytes[256U];
    uint8_t read_wire_bytes[256U];
    uint8_t response_bytes[1024U];
    size_t association_length = 0U;
    size_t read_wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    memset(&logical_device, 0, sizeof(logical_device));
    memset(&model_plan, 0, sizeof(model_plan));
    snprintf(logical_device.inst, sizeof(logical_device.inst), "%s", "IED1LD0");
    model_plan.logical_device_count = 1U;
    model_plan.logical_devices = &logical_device;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &model_plan) == 1);
    assert(build_initiate_request_association_bytes(association_bytes, sizeof(association_bytes), &association_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, association_bytes, association_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_session_complete_association(&server_runtime.session, server_runtime.session.active_invoke_id, &diagnostic));

    assert(build_model_read_request_association_bytes("LD0$LLN0$BR", 15U, read_wire_bytes, sizeof(read_wire_bytes), &read_wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, read_wire_bytes, read_wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == read_wire_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_READ);
    assert(server_runtime.pending_request.invoke_id == 15U);
    assert(strcmp(server_runtime.pending_request.object_reference, "LD0.LLN0.BR") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
    assert_read_response_failure_access_result(response_bytes, response_length, 15U);
}

static void test_server_runtime_build_brcb_read_uses_default_advertised_domain(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t response_bytes[1024U];
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 18U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "LD0.LLN0.BR.brcbEvents");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "brcbEvents");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"IED1LD0/LLN0.BR.Events", strlen("IED1LD0/LLN0.BR.Events")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"IED1LD0/LLN0$dsEvents", strlen("IED1LD0/LLN0$dsEvents")) == 1);
}

static void test_server_runtime_build_brcb_scalar_multi_read_uses_direct_data_access_results(void)
{
    static const char* const fields[] = {
        "RptID",
        "RptEna",
        "DatSet",
        "ConfRev",
        "OptFlds",
        "BufTm",
        "SqNum",
        "TrgOps",
        "IntgPd",
        "GI",
        "PurgeBuf",
        "EntryID",
        "TimeofEntry",
        "ResvTms"
    };
    static const uint32_t expected_tags[] = { 10U, 3U, 10U, 6U, 4U, 6U, 6U, 4U, 6U, 3U, 3U, 9U, 12U, 5U };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelLogicalDevice logical_device;
    UnitLabIedModelPlan model_plan;
    uint8_t response_bytes[1024U];
    size_t response_length = 0U;

    memset(&logical_device, 0, sizeof(logical_device));
    memset(&model_plan, 0, sizeof(model_plan));
    snprintf(logical_device.inst, sizeof(logical_device.inst), "%s", "IED1LD0");
    model_plan.logical_device_count = 1U;
    model_plan.logical_devices = &logical_device;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &model_plan) == 1);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 16U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "LD0.LLN0.BR.brcbEvents.RptID");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "RptID");
    server_runtime.pending_request.read_object_reference_count = sizeof(fields) / sizeof(fields[0]);
    for (size_t index = 0U; index < sizeof(fields) / sizeof(fields[0]); index++) {
        snprintf(server_runtime.pending_request.read_object_references[index], sizeof(server_runtime.pending_request.read_object_references[index]), "LD0.LLN0.BR.brcbEvents.%s", fields[index]);
        snprintf(server_runtime.pending_request.read_attribute_references[index], sizeof(server_runtime.pending_request.read_attribute_references[index]), "%s", fields[index]);
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert_read_response_access_result_tags(response_bytes, response_length, 16U, expected_tags, sizeof(expected_tags) / sizeof(expected_tags[0]));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"IED1LD0/LLN0$dsEvents", strlen("IED1LD0/LLN0$dsEvents")) == 1);
}


static void test_server_runtime_brcb_read_uses_model_report_dataset_reference(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedFixtureDataSet data_sets[1U] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 0U,
            .signals = NULL,
        },
    };
    UnitLabIedFixtureReport reports[1U] = {
        {
            .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
            .logical_device_inst = "LD0",
            .logical_node_name = "LLN0",
            .report_control_name = "brcbEvents",
            .report_kind = "buffered",
            .rpt_id = "IED1LD0/LLN0.BR.Events",
            .data_set_ref = "IED1/AP1/LD0/LLN0.dsEvents",
            .conf_rev = "7",
            .indexed_known = 1,
            .indexed = 0,
            .buffer_time_ms_known = 1,
            .buffer_time_ms = 100,
            .integrity_period_ms_known = 1,
            .integrity_period_ms = 1000,
        },
    };
    UnitLabIedFixtureModel fixture = {
        .device_count = 1U,
        .ied_name = "IED1",
        .access_point_name = "AP1",
        .data_set_count = 1U,
        .data_sets = data_sets,
        .report_count = 1U,
        .reports = reports,
        .signal_count = 0U,
    };
    UnitLabIedModelPlan plan;
    char error[256U];
    uint8_t response_bytes[1024U];
    size_t response_length = 0U;

    assert(unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error)) == 1);

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 22U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "IED1LD0.LLN0.BR.brcbEvents");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "brcbEvents");
    server_runtime.pending_request.read_object_reference_count = 1U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "IED1LD0.LLN0.BR.brcbEvents");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "brcbEvents");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"IED1LD0/LLN0.BR.Events", strlen("IED1LD0/LLN0.BR.Events")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"IED1LD0/LLN0$dsEvents", strlen("IED1LD0/LLN0$dsEvents")) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"IED1IED1LD0", strlen("IED1IED1LD0")) == 0);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_build_read_failure_uses_data_access_error_access_result(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t response_bytes[256U];
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 17U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "LD0.LLN0.BR.brcbEvents.Unknown");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "Unknown");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert_read_response_failure_access_result(response_bytes, response_length, 17U);
}

static void test_server_runtime_model_placeholder_reads_keep_discovery_flow_alive(void)
{
    static const uint32_t expected_tags[] = { 2U, 7U };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t response_bytes[512U];
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 35U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "KINTE06SMPCTRL.GGIO1.MX");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "MX");
    server_runtime.pending_request.read_object_reference_count = 2U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "KINTE06SMPCTRL.GGIO1.MX");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "MX");
    snprintf(server_runtime.pending_request.read_object_references[1], sizeof(server_runtime.pending_request.read_object_references[1]), "%s", "KINTE06SMPCTRL.GGIO1.MX.AnIn1.mag.f");
    snprintf(server_runtime.pending_request.read_attribute_references[1], sizeof(server_runtime.pending_request.read_attribute_references[1]), "%s", "f");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert_read_response_access_result_tags(response_bytes, response_length, 35U, expected_tags, sizeof(expected_tags) / sizeof(expected_tags[0]));
}

static void test_server_runtime_mixed_multi_read_preserves_access_result_failures(void)
{
    static const uint32_t expected_tags[] = { 10U, 0U };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t response_bytes[512U];
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 34U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "LD0.LLN0.DC.NamPlt.vendor");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "vendor");
    server_runtime.pending_request.read_object_reference_count = 2U;
    snprintf(server_runtime.pending_request.read_object_references[0], sizeof(server_runtime.pending_request.read_object_references[0]), "%s", "LD0.LLN0.DC.NamPlt.vendor");
    snprintf(server_runtime.pending_request.read_attribute_references[0], sizeof(server_runtime.pending_request.read_attribute_references[0]), "%s", "vendor");
    snprintf(server_runtime.pending_request.read_object_references[1], sizeof(server_runtime.pending_request.read_object_references[1]), "%s", "LD0.LLN0.BR.brcbEvents.Unknown");
    snprintf(server_runtime.pending_request.read_attribute_references[1], sizeof(server_runtime.pending_request.read_attribute_references[1]), "%s", "Unknown");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert_read_response_access_result_tags(response_bytes, response_length, 34U, expected_tags, sizeof(expected_tags) / sizeof(expected_tags[0]));
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"UnitLab", strlen("UnitLab")) == 1);
}


static void test_server_runtime_build_ordinary_ln_gva_response_exposes_fc_roots(void)
{
    static const struct {
        const char* object_reference;
        const char* attribute_reference;
        const char* fc;
        const char* child_a;
        const char* child_b;
        const char* child_c;
    } cases[] = {
        { "LD0.XCBR1", "XCBR1", "ST", "Pos", "stVal", NULL },
        { "LD0.PGGIO1", "PGGIO1", "ST", "Ind1", "stVal", NULL },
        { "LD0.GGIO1", "GGIO1", "MX", "AnIn1", "mag", "f" }
    };

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++) {
        UnitLabMmsServerRuntime server_runtime;
        UnitLabMmsDiagnostic diagnostic;
        UnitLabIedServerConfig config = {
            .bind_address = "127.0.0.1",
            .port = 102,
        };
        uint8_t response_bytes[4096U];
        size_t response_length = 0U;
        size_t response_consumed_length = 0U;
        UnitLabMmsAssociationFrame frame;

        unitlab_mms_server_runtime_init(&server_runtime);
        assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
        assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
        assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES, (uint32_t)(30U + index), 7U, 1000U, 100U, &diagnostic) == 1);
        snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", cases[index].object_reference);
        snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", cases[index].attribute_reference);

        unitlab_mms_diagnostic_clear(&diagnostic);
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
        assert(response_length > 0U);

        unitlab_mms_association_frame_init(&frame);
        assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)cases[index].fc, strlen(cases[index].fc)) == 1);
        assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)cases[index].child_a, strlen(cases[index].child_a)) == 1);
        assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)cases[index].child_b, strlen(cases[index].child_b)) == 1);
        if (cases[index].child_c != NULL) {
            assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)cases[index].child_c, strlen(cases[index].child_c)) == 1);
            assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t[]){ 0x80U, 0x01U, 0x66U, 0xA1U, 0x03U, 0x85U, 0x01U, 0x20U }, 8U) == 1);
        }
    }
}

static void test_server_runtime_build_model_fc_root_gva_response_exposes_dataset_do_tree(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    UnitLabIedModelSignal signals[5U];
    uint8_t response_bytes[16384U];
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;
    UnitLabMmsAssociationFrame frame;

    memset(&plan, 0, sizeof(plan));
    memset(signals, 0, sizeof(signals));
    for (size_t index = 0U; index < 5U; index++) {
        snprintf(signals[index].logical_device_inst, sizeof(signals[index].logical_device_inst), "%s", "KINTE15FMPSystem");
        snprintf(signals[index].logical_node_name, sizeof(signals[index].logical_node_name), "%s", "RlyGGIO1");
        snprintf(signals[index].data_object_name, sizeof(signals[index].data_object_name), "%s", "Ind9");
        snprintf(signals[index].fc, sizeof(signals[index].fc), "%s", "ST");
        signals[index].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER;
        snprintf(signals[index].initial_value, sizeof(signals[index].initial_value), "%s", "0");
    }
    signals[0].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_BOOLEAN;
    snprintf(signals[0].initial_value, sizeof(signals[0].initial_value), "%s", "false");
    snprintf(signals[0].data_attribute_path, sizeof(signals[0].data_attribute_path), "%s", "stVal");
    snprintf(signals[0].data_set_entry_variable, sizeof(signals[0].data_set_entry_variable), "%s", "KINTE15FMPSystem/RlyGGIO1$ST$Ind9$stVal");
    snprintf(signals[1].data_attribute_path, sizeof(signals[1].data_attribute_path), "%s", "q");
    snprintf(signals[1].data_set_entry_variable, sizeof(signals[1].data_set_entry_variable), "%s", "KINTE15FMPSystem/RlyGGIO1$ST$Ind9$q");
    signals[2].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
    snprintf(signals[2].initial_value, sizeof(signals[2].initial_value), "%s", "");
    snprintf(signals[2].data_attribute_path, sizeof(signals[2].data_attribute_path), "%s", "t");
    snprintf(signals[2].data_set_entry_variable, sizeof(signals[2].data_set_entry_variable), "%s", "KINTE15FMPSystem/RlyGGIO1$ST$Ind9$t");
    snprintf(signals[3].data_object_name, sizeof(signals[3].data_object_name), "%s", "Str");
    snprintf(signals[3].data_attribute_path, sizeof(signals[3].data_attribute_path), "%s", "dirGeneral");
    snprintf(signals[3].data_set_entry_variable, sizeof(signals[3].data_set_entry_variable), "%s", "KINTE15FMPSystem/RlyGGIO1$ST$Str$dirGeneral");
    signals[4].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_REAL;
    snprintf(signals[4].initial_value, sizeof(signals[4].initial_value), "%s", "0.0");
    snprintf(signals[4].data_object_name, sizeof(signals[4].data_object_name), "%s", "Ana");
    snprintf(signals[4].data_attribute_path, sizeof(signals[4].data_attribute_path), "%s", "mag.f");
    snprintf(signals[4].data_set_entry_variable, sizeof(signals[4].data_set_entry_variable), "%s", "KINTE15FMPSystem/RlyGGIO1$ST$Ana$mag$f");
    plan.signal_count = 5U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES, 77U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "KINTE15FMPSystem.RlyGGIO1.ST");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "ST");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&frame);
    assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"Ind9", strlen("Ind9")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"stVal", strlen("stVal")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"q", strlen("q")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"t", strlen("t")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"Str", strlen("Str")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"dirGeneral", strlen("dirGeneral")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"Ana", strlen("Ana")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"mag", strlen("mag")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"f", strlen("f")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t[]){ 0x83U, 0x00U }, 2U) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t[]){ 0x80U, 0x01U, 0x71U, 0xA1U, 0x03U, 0x84U, 0x01U, 0x0DU }, 8U) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t[]){ 0x80U, 0x01U, 0x74U, 0xA1U, 0x02U, 0x91U, 0x00U }, 7U) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t[]){ 0x85U, 0x01U, 0x20U }, 3U) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t[]){ 0xA7U, 0x06U, 0x02U, 0x01U, 0x20U, 0x02U, 0x01U, 0x08U }, 8U) == 1);
}


static void test_server_runtime_build_model_lln0_gva_uses_report_class_presence(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t response_bytes[8192U];
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;
    UnitLabMmsAssociationFrame frame;

    assert(build_two_model_backed_rcb_plan(&plan) == 1);
    for (size_t index = 0U; index < plan.report_count; index++) {
        plan.reports[index].is_buffered = 0;
        snprintf(plan.reports[index].report_kind, sizeof(plan.reports[index].report_kind), "%s", "unbuffered");
    }

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES, 81U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "KINTE08TDIFFSystem.LLN0");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "LLN0");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&frame);
    assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"RP", strlen("RP")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"brcbA", strlen("brcbA")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"brcbB", strlen("brcbB")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"BR", strlen("BR")) == 0);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"brcbEvents", strlen("brcbEvents")) == 0);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES, 82U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "KINTE08TDIFFSystem.LLN0.RP.brcbA");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "brcbA");

    response_length = 0U;
    response_consumed_length = 0U;
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&frame);
    assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"RptID", strlen("RptID")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"Resv", strlen("Resv")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"DatSet", strlen("DatSet")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t[]){ 0x80U, 0x05U, 0x53U, 0x71U, 0x4EU, 0x75U, 0x6DU, 0xA1U, 0x03U, 0x86U, 0x01U, 0x08U }, 12U) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"PurgeBuf", strlen("PurgeBuf")) == 0);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"EntryID", strlen("EntryID")) == 0);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"ResvTms", strlen("ResvTms")) == 0);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_build_model_report_gva_response_uses_report_control_names(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    uint8_t response_bytes[8192U];
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;
    UnitLabMmsAssociationFrame frame;

    assert(build_two_model_backed_rcb_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES, 78U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "KINTE08TDIFFSystem.LLN0");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "LLN0");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&frame);
    assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"BR", strlen("BR")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"brcbA", strlen("brcbA")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"brcbB", strlen("brcbB")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"brcbEvents", strlen("brcbEvents")) == 0);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES, 79U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "KINTE08TDIFFSystem.LLN0.BR");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "BR");

    response_length = 0U;
    response_consumed_length = 0U;
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&frame);
    assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"brcbA", strlen("brcbA")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"brcbB", strlen("brcbB")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"brcbEvents", strlen("brcbEvents")) == 0);
    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 0U, &diagnostic));

    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES, 80U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "KINTE08TDIFFSystem.LLN0.BR.brcbA");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "brcbA");

    response_length = 0U;
    response_consumed_length = 0U;
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&frame);
    assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"RptID", strlen("RptID")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"DatSet", strlen("DatSet")) == 1);
    assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"TrgOps", strlen("TrgOps")) == 1);

    unitlab_free_ied_model_plan(&plan);
}


static void test_server_runtime_build_fc_root_reads_match_lib_shape(void)
{
    static const uint8_t ggio1_mx_expected[] = { 0xA2U, 0x07U, 0xA2U, 0x05U, 0xA2U, 0x03U, 0x85U, 0x01U, 0x00U };
    static const uint8_t pggio1_st_expected[] = { 0xA2U, 0x05U, 0xA2U, 0x03U, 0x85U, 0x01U, 0x01U };
    static const uint8_t xcbr1_st_expected[] = { 0xA2U, 0x05U, 0xA2U, 0x03U, 0x85U, 0x01U, 0x00U };
    static const struct {
        const char* object_reference;
        const char* attribute_reference;
        const uint8_t* expected_bytes;
        size_t expected_length;
    } cases[] = {
        { "LD0.GGIO1.MX", "MX", ggio1_mx_expected, sizeof(ggio1_mx_expected) },
        { "LD0.PGGIO1.ST", "ST", pggio1_st_expected, sizeof(pggio1_st_expected) },
        { "LD0.XCBR1.ST", "ST", xcbr1_st_expected, sizeof(xcbr1_st_expected) }
    };

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++) {
        UnitLabMmsServerRuntime server_runtime;
        UnitLabMmsDiagnostic diagnostic;
        UnitLabIedServerConfig config = {
            .bind_address = "127.0.0.1",
            .port = 102,
        };
        UnitLabIedModelPlan plan;
        uint8_t response_bytes[512U];
        uint8_t pggio_value = 0x01U;
        size_t response_length = 0U;

        assert(build_runtime_value_store_plan(&plan) == 1);
        unitlab_mms_server_runtime_init(&server_runtime);
        assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
        assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
        assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
        assert(unitlab_mms_server_runtime_update_signal_value(&server_runtime, "IED1LD0/PGGIO1$ST$Ind1$stVal", &pggio_value, 1U, &diagnostic));
        assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, (uint32_t)(40U + index), 7U, 1000U, 100U, &diagnostic) == 1);
        snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", cases[index].object_reference);
        snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", cases[index].attribute_reference);

        unitlab_mms_diagnostic_clear(&diagnostic);
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
        assert(response_length > 0U);
        assert(contains_bytes(response_bytes, response_length, cases[index].expected_bytes, cases[index].expected_length) == 1);

        unitlab_free_ied_model_plan(&plan);
    }
}

static void test_server_runtime_build_brcb_gva_response_exposes_fields(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t response_bytes[4096U];
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES, 15U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "LD0.LLN0.BR.brcbEvents");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "LLN0.BR.brcbEvents");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"RptID", strlen("RptID")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"RptEna", strlen("RptEna")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"DatSet", strlen("DatSet")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"ConfRev", strlen("ConfRev")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"OptFlds", strlen("OptFlds")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"BufTm", strlen("BufTm")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"SqNum", strlen("SqNum")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"TrgOps", strlen("TrgOps")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"IntgPd", strlen("IntgPd")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"GI", strlen("GI")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"PurgeBuf", strlen("PurgeBuf")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"EntryID", strlen("EntryID")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"TimeofEntry", strlen("TimeofEntry")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"ResvTms", strlen("ResvTms")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t[]){ 0x80U, 0x07U, 'R', 'e', 's', 'v', 'T', 'm', 's', 0xA1U, 0x03U, 0x85U, 0x01U, 0x10U }, 14U) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Owner", strlen("Owner")) == 0);
    }
}

static void test_server_runtime_build_get_name_list_response_handles_large_directory(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[24U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[4096U];
    const uint8_t request_payload[] = {
        0x30U, 0x09U,
        0xA0U, 0x03U, 0x02U, 0x01U, 0x09U,
        0xA1U, 0x02U, 0x80U, 0x00U
    };
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    for (size_t index = 0U; index < sizeof(logical_devices) / sizeof(logical_devices[0]); index++) {
        snprintf(logical_devices[index].inst, sizeof(logical_devices[index].inst), "LD%02u", (unsigned)index);
    }
    plan.logical_device_count = sizeof(logical_devices) / sizeof(logical_devices[0]);
    plan.logical_devices = logical_devices;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(build_get_name_list_request_association_bytes(request_payload, sizeof(request_payload), 61U, request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == request_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 9U);
    assert(server_runtime.pending_request.browse_object_scope == 0U);

    unitlab_mms_diagnostic_clear(&diagnostic);
    if (!unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic)) {
        assert(0);
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 64U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"LD00", strlen("LD00")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"LD23", strlen("LD23")) == 1);
    }
}

static void test_server_runtime_build_confirmed_response_bytes_matches_fixture_style_object_reference(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelSignal signals[1U];
    uint8_t wire_bytes[256];
    uint8_t response_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(signals, 0, sizeof(signals));
    strcpy(signals[0].object_reference, "Pos.stVal");
    signals[0].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER;
    strcpy(signals[0].initial_value, "0");
    plan.signal_count = 1U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(build_model_read_request_association_bytes("XCBR1$ST$Pos$stVal", 3U, wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(strcmp(server_runtime.pending_request.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x85U, 0x01U, 0x00U }, 3U) == 1);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x00U }, 3U) == 0);
}

static void test_server_runtime_apply_confirmed_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    uint8_t response_bytes[256];
    uint8_t response_payload[5U] = { 0x02U, 0x01U, 0x05U, 0xA4U, 0x00U };
    UnitLabMmsAssociationFrame fixture;
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 5U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "XCBR1.ST.Pos.stVal");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "stVal");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, response_payload, sizeof(response_payload), response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&fixture);
    assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    {
        UnitLabMmsPdu decoded_response_pdu;
        size_t response_pdu_consumed_length = 0U;

        unitlab_mms_pdu_init(&decoded_response_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_response_pdu, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &response_pdu_consumed_length, &diagnostic));
        assert(response_pdu_consumed_length == fixture.presentation.payload_length);
        assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response_pdu.has_invoke_id == 1);
        assert(decoded_response_pdu.invoke_id == 5U);
        assert(decoded_response_pdu.has_service == 1);
        assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
        assert(decoded_response_pdu.pdu_length == sizeof(response_payload));
        assert(memcmp(decoded_response_pdu.pdu_bytes, response_payload, sizeof(response_payload)) == 0);
    }

    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 1234U, &diagnostic));
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_COMPLETED);

    (void)wire_bytes;
    (void)wire_length;
    (void)consumed_length;
    (void)operation_result;
    (void)response_consumed_length;
}

static void test_server_runtime_rejects_mismatched_confirmed_response_invoke_id(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 5U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "XCBR1.ST.Pos.stVal");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "stVal");

    unitlab_mms_pdu_init(&wire_pdu);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 7U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_READ;

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_wire_pdu(&server_runtime, &wire_pdu, &operation_result) == 0);
    assert(operation_result.ok == 0);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVOKE_ID_MISMATCH);
    assert(operation_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_CORRELATION_MISMATCH);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
}

static void test_server_runtime_confirmed_response_fails_after_timeout(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t response_bytes[128U];
    uint8_t response_payload[5U] = { 0x02U, 0x01U, 0x05U, 0xA4U, 0x00U };
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 11U, 7U, 1000U, 100U, &diagnostic) == 1);
    assert(unitlab_mms_pending_request_mark_timed_out(&server_runtime.pending_request, 1100U, &diagnostic) == 1);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_TIMED_OUT);
    assert(server_runtime.pending_request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_TIMED_OUT);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, response_payload, sizeof(response_payload), response_bytes, sizeof(response_bytes), &response_length, &diagnostic) == 0);
    assert(response_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BAD_STATE);
}



static void test_server_runtime_apply_pcap_resvtms_write_builds_response(void)
{
    static const uint8_t wire_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x4DU, 0x02U, 0xF0U, 0x80U, 0x01U,
        0x00U, 0x01U, 0x00U, 0x61U, 0x40U, 0x30U, 0x3EU, 0x02U,
        0x01U, 0x03U, 0xA0U, 0x39U, 0xA0U, 0x37U, 0x02U, 0x01U,
        0x13U, 0xA5U, 0x32U, 0xA0U, 0x2BU, 0x30U, 0x29U, 0xA0U,
        0x27U, 0xA1U, 0x25U, 0x1AU, 0x07U, 'I',  'E',  'D',  '1',
        'L',  'D',  '0',  0x1AU, 0x1AU, 'L',  'L',  'N',  '0',  '$',
        'B',  'R',  '$',  'b',  'r',  'c',  'b',  'E',  'v',  'e',
        'n',  't',  's',  '$',  'R',  'e',  's',  'v',  'T',  'm',
        's',  0xA0U, 0x03U, 0x85U, 0x01U, 0x2AU
    };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t response_bytes[256U];
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, sizeof(wire_bytes), &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(wire_bytes));
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_WRITE);
    assert(server_runtime.pending_request.invoke_id == 19U);
    assert(strcmp(server_runtime.pending_request.object_reference, "IED1LD0.LLN0.BR.brcbEvents.ResvTms") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
    assert(server_runtime.brcb_resv_tms == 42U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x13U, 0xA5U, 0x02U, 0x81U, 0x00U }, 7U) == 1);
}

static void test_server_runtime_apply_pcap_resvtms_write_rejects_when_enabled(void)
{
    static const uint8_t wire_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x4DU, 0x02U, 0xF0U, 0x80U, 0x01U,
        0x00U, 0x01U, 0x00U, 0x61U, 0x40U, 0x30U, 0x3EU, 0x02U,
        0x01U, 0x03U, 0xA0U, 0x39U, 0xA0U, 0x37U, 0x02U, 0x01U,
        0x13U, 0xA5U, 0x32U, 0xA0U, 0x2BU, 0x30U, 0x29U, 0xA0U,
        0x27U, 0xA1U, 0x25U, 0x1AU, 0x07U, 'I',  'E',  'D',  '1',
        'L',  'D',  '0',  0x1AU, 0x1AU, 'L',  'L',  'N',  '0',  '$',
        'B',  'R',  '$',  'b',  'r',  'c',  'b',  'E',  'v',  'e',
        'n',  't',  's',  '$',  'R',  'e',  's',  'v',  'T',  'm',
        's',  0xA0U, 0x03U, 0x85U, 0x01U, 0x2AU
    };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t response_bytes[256U];
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_resv_tms = 30U;
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, sizeof(wire_bytes), &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(wire_bytes));
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_WRITE);
    assert(server_runtime.pending_request.invoke_id == 19U);
    assert(strcmp(server_runtime.pending_request.object_reference, "IED1LD0.LLN0.BR.brcbEvents.ResvTms") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
    assert(server_runtime.brcb_resv_tms == 30U);
    assert(server_runtime.brcb_rpt_ena == 1U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x13U, 0xA5U, 0x03U, 0x80U, 0x01U, 0x09U }, 8U) == 1);
}

static void test_server_runtime_apply_pcap_multi_rptena_write_builds_response(void)
{
    static const uint8_t wire_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x8EU, 0x02U, 0xF0U, 0x80U, 0x01U,
        0x00U, 0x01U, 0x00U, 0x61U, 0x81U, 0x80U, 0x30U, 0x7EU,
        0x02U, 0x01U, 0x03U, 0xA0U, 0x79U, 0xA0U, 0x77U, 0x02U,
        0x01U, 0x14U, 0xA5U, 0x72U, 0xA0U, 0x54U, 0x30U, 0x28U,
        0xA0U, 0x26U, 0xA1U, 0x24U, 0x1AU, 0x07U, 0x49U, 0x45U,
        0x44U, 0x31U, 0x4CU, 0x44U, 0x30U, 0x1AU, 0x19U, 0x4CU,
        0x4CU, 0x4EU, 0x30U, 0x24U, 0x42U, 0x52U, 0x24U, 0x62U,
        0x72U, 0x63U, 0x62U, 0x45U, 0x76U, 0x65U, 0x6EU, 0x74U,
        0x73U, 0x24U, 0x44U, 0x61U, 0x74U, 0x53U, 0x65U, 0x74U,
        0x30U, 0x28U, 0xA0U, 0x26U, 0xA1U, 0x24U, 0x1AU, 0x07U,
        0x49U, 0x45U, 0x44U, 0x31U, 0x4CU, 0x44U, 0x30U, 0x1AU,
        0x19U, 0x4CU, 0x4CU, 0x4EU, 0x30U, 0x24U, 0x42U, 0x52U,
        0x24U, 0x62U, 0x72U, 0x63U, 0x62U, 0x45U, 0x76U, 0x65U,
        0x6EU, 0x74U, 0x73U, 0x24U, 0x52U, 0x70U, 0x74U, 0x45U,
        0x6EU, 0x61U, 0xA0U, 0x1AU, 0x8AU, 0x15U, 0x49U, 0x45U,
        0x44U, 0x31U, 0x4CU, 0x44U, 0x30U, 0x2FU, 0x4CU, 0x4CU,
        0x4EU, 0x30U, 0x24U, 0x64U, 0x73U, 0x45U, 0x76U, 0x65U,
        0x6EU, 0x74U, 0x73U, 0x83U, 0x01U, 0x01U
    };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t response_bytes[256U];
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, sizeof(wire_bytes), &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(wire_bytes));
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_WRITE);
    assert(server_runtime.pending_request.invoke_id == 20U);
    assert(server_runtime.pending_request.write_object_reference_count == 2U);
    assert(strcmp(server_runtime.pending_request.write_object_references[0], "IED1LD0.LLN0.BR.brcbEvents.DatSet") == 0);
    assert(strcmp(server_runtime.pending_request.write_object_references[1], "IED1LD0.LLN0.BR.brcbEvents.RptEna") == 0);
    assert(server_runtime.pending_request.write_value_lengths[0] == strlen("IED1LD0/LLN0$dsEvents"));
    assert(server_runtime.pending_request.write_value_lengths[1] == 1U);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
    assert(server_runtime.brcb_rpt_ena == 1U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x14U, 0xA5U, 0x04U, 0x81U, 0x00U, 0x81U, 0x00U }, 9U) == 1);
}

static void test_server_runtime_apply_pcap_multi_write_preserves_partial_failures(void)
{
    static const uint8_t wire_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x8EU, 0x02U, 0xF0U, 0x80U, 0x01U,
        0x00U, 0x01U, 0x00U, 0x61U, 0x81U, 0x80U, 0x30U, 0x7EU,
        0x02U, 0x01U, 0x03U, 0xA0U, 0x79U, 0xA0U, 0x77U, 0x02U,
        0x01U, 0x14U, 0xA5U, 0x72U, 0xA0U, 0x54U, 0x30U, 0x28U,
        0xA0U, 0x26U, 0xA1U, 0x24U, 0x1AU, 0x07U, 0x49U, 0x45U,
        0x44U, 0x31U, 0x4CU, 0x44U, 0x30U, 0x1AU, 0x19U, 0x4CU,
        0x4CU, 0x4EU, 0x30U, 0x24U, 0x42U, 0x52U, 0x24U, 0x62U,
        0x72U, 0x63U, 0x62U, 0x45U, 0x76U, 0x65U, 0x6EU, 0x74U,
        0x73U, 0x24U, 0x44U, 0x61U, 0x74U, 0x53U, 0x65U, 0x74U,
        0x30U, 0x28U, 0xA0U, 0x26U, 0xA1U, 0x24U, 0x1AU, 0x07U,
        0x49U, 0x45U, 0x44U, 0x31U, 0x4CU, 0x44U, 0x30U, 0x1AU,
        0x19U, 0x4CU, 0x4CU, 0x4EU, 0x30U, 0x24U, 0x42U, 0x52U,
        0x24U, 0x62U, 0x72U, 0x63U, 0x62U, 0x45U, 0x76U, 0x65U,
        0x6EU, 0x74U, 0x73U, 0x24U, 0x52U, 0x70U, 0x74U, 0x45U,
        0x6EU, 0x61U, 0xA0U, 0x1AU, 0x8AU, 0x15U, 0x49U, 0x45U,
        0x44U, 0x31U, 0x4CU, 0x44U, 0x30U, 0x2FU, 0x4CU, 0x4CU,
        0x4EU, 0x30U, 0x24U, 0x64U, 0x73U, 0x45U, 0x76U, 0x65U,
        0x6EU, 0x74U, 0x73U, 0x83U, 0x01U, 0x01U
    };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    uint8_t response_bytes[256U];
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, sizeof(wire_bytes), &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(wire_bytes));
    assert(server_runtime.pending_request.write_object_reference_count == 2U);
    assert(strcmp(server_runtime.pending_request.write_object_references[0], "IED1LD0.LLN0.BR.brcbEvents.DatSet") == 0);
    assert(strcmp(server_runtime.pending_request.write_object_references[1], "IED1LD0.LLN0.BR.brcbEvents.RptEna") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
    assert(server_runtime.brcb_rpt_ena == 1U);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t[]){ 0x02U, 0x01U, 0x14U, 0xA5U, 0x05U, 0x80U, 0x01U, 0x09U, 0x81U, 0x00U }, 10U) == 1);
}

static void test_server_runtime_apply_write_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    UnitLabMmsAssociationFrame fixture;
    uint8_t response_bytes[256];
    uint8_t response_payload[7U] = { 0x02U, 0x01U, 0x2BU, 0xA5U, 0x02U, 0x81U, 0x00U };
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));

    {
        UnitLabMmsAssociationFrame request_fixture;
        UnitLabMmsPdu request_pdu;
        uint8_t request_payload[] = { 0x02U, 0x01U, 0x2BU, 0xA5U, 0x24U, 0x30U, 0x22U, 0xA0U, 0x1BU, 0x30U, 0x19U, 0xA0U, 0x17U, 0xA1U, 0x15U, 0x1AU, 0x05U, 'X', 'C', 'B', 'R', '1', 0x1AU, 0x0CU, 'S', 'T', '$', 'P', 'o', 's', '$', 's', 't', 'V', 'a', 'l', 0xA0U, 0x03U, 0x83U, 0x01U, 0xFFU };
        uint8_t request_encoded[256U];
        size_t request_length = 0U;
        size_t request_frame_length = 0U;

        unitlab_mms_pdu_init(&request_pdu);
        request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
        request_pdu.pdu_bytes = request_payload;
        request_pdu.pdu_length = sizeof(request_payload);
        assert(unitlab_mms_pdu_encode(&request_pdu, request_encoded, sizeof(request_encoded), &request_length, &diagnostic));
        unitlab_mms_association_frame_init(&request_fixture);
        request_fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
        request_fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
        request_fixture.presentation.payload_bytes = request_encoded;
        request_fixture.presentation.payload_length = request_length;
        request_fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
        assert(unitlab_mms_association_frame_encode(&request_fixture, wire_bytes, sizeof(wire_bytes), &request_frame_length, &diagnostic));
        wire_length = request_frame_length;
    }

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_WRITE);
    assert(server_runtime.pending_request.invoke_id == 43U);
    assert(server_runtime.pending_request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, response_payload, sizeof(response_payload), response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&fixture);
    assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    {
        UnitLabMmsPdu decoded_response_pdu;
        size_t response_pdu_consumed_length = 0U;

        unitlab_mms_pdu_init(&decoded_response_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_response_pdu, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &response_pdu_consumed_length, &diagnostic));
        assert(response_pdu_consumed_length == fixture.presentation.payload_length);
        assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response_pdu.has_invoke_id == 1);
        assert(decoded_response_pdu.invoke_id == 43U);
        assert(decoded_response_pdu.has_service == 1);
        assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_WRITE);
        assert(decoded_response_pdu.pdu_length == sizeof(response_payload));
        assert(memcmp(decoded_response_pdu.pdu_bytes, response_payload, sizeof(response_payload)) == 0);
    }
}

static int build_get_name_list_request_association_bytes(const uint8_t* request_body_bytes, size_t request_body_length, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsAssociationFrame fixture;
    UnitLabMmsPdu pdu;
    UnitLabMmsBerElement service_element;
    UnitLabMmsBerElement invoke_id_element;
    uint8_t service_bytes[128U];
    uint8_t request_payload[192U];
    uint8_t request_encoded[224U];
    size_t service_length = 0U;
    size_t invoke_id_length = 0U;
    size_t request_length = 0U;
    size_t frame_length = 0U;

    unitlab_mms_ber_element_init(&service_element);
    service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    service_element.tag.constructed = 1;
    service_element.tag.tag_number = 1U;
    service_element.value_bytes = request_body_bytes;
    service_element.value_length = request_body_length;
    if (!unitlab_mms_ber_write(&service_element, service_bytes, sizeof(service_bytes), &service_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&invoke_id_element);
    invoke_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    invoke_id_element.tag.constructed = 0;
    invoke_id_element.tag.tag_number = 2U;
    {
        uint8_t invoke_id_raw[5U];
        size_t invoke_id_raw_length = 0U;
        uint32_t value = invoke_id;

        do {
            invoke_id_raw[sizeof(invoke_id_raw) - 1U - invoke_id_raw_length] = (uint8_t)(value & 0xFFU);
            invoke_id_raw_length++;
            value >>= 8U;
        } while (value != 0U && invoke_id_raw_length < sizeof(invoke_id_raw));
        if (invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length] & 0x80U) {
            if (sizeof(invoke_id_raw) == invoke_id_raw_length) {
                if (diagnostic != NULL) {
                    diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
                }
                return 0;
            }
            invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length - 1U] = 0x00U;
            invoke_id_raw_length++;
        }
        invoke_id_element.value_bytes = &invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length];
        invoke_id_element.value_length = invoke_id_raw_length;
        if (!unitlab_mms_ber_write(&invoke_id_element, request_payload, sizeof(request_payload), &invoke_id_length, diagnostic)) {
            return 0;
        }
    }

    if (invoke_id_length + service_length > sizeof(request_payload)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
        }
        return 0;
    }
    memcpy(request_payload + invoke_id_length, service_bytes, service_length);
    request_length = invoke_id_length + service_length;

    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    pdu.pdu_bytes = request_payload;
    pdu.pdu_length = request_length;
    if (!unitlab_mms_pdu_encode(&pdu, request_encoded, sizeof(request_encoded), &request_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_association_frame_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    fixture.presentation.payload_bytes = request_encoded;
    fixture.presentation.payload_length = request_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;

    if (!unitlab_mms_association_frame_encode(&fixture, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }
    *encoded_length = frame_length;
    return 1;
}

static void test_server_runtime_apply_release_request_builds_lib_shape_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    static const uint8_t release_request_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x16U, 0x02U, 0xF0U, 0x80U, 0x01U,
        0x00U, 0x01U, 0x00U, 0x61U, 0x09U, 0x30U, 0x07U, 0x02U,
        0x01U, 0x03U, 0xA0U, 0x02U, 0x8BU, 0x00U,
    };
    static const uint8_t expected_release_response_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x16U, 0x02U, 0xF0U, 0x80U, 0x01U,
        0x00U, 0x01U, 0x00U, 0x61U, 0x09U, 0x30U, 0x07U, 0x02U,
        0x01U, 0x03U, 0xA0U, 0x02U, 0x8CU, 0x00U,
    };
    uint8_t response_bytes[256U];
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, server_runtime.session.active_invoke_id, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, release_request_bytes, sizeof(release_request_bytes), &consumed_length, &operation_result) == 1);
    assert(consumed_length == sizeof(release_request_bytes));
    assert(operation_result.ok == 1);
    assert(server_runtime.last_wire_pdu.kind == UNITLAB_MMS_PDU_CONCLUDE_REQUEST);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_RELEASING);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_release_response_bytes(&server_runtime, response_bytes, sizeof(response_bytes), &response_length, &diagnostic) == 1);
    assert(response_length == sizeof(expected_release_response_bytes));
    assert(memcmp(response_bytes, expected_release_response_bytes, sizeof(expected_release_response_bytes)) == 0);
    assert(unitlab_mms_session_complete_release(&server_runtime.session, &diagnostic) == 1);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
}

static void test_server_runtime_apply_get_name_list_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[2U];
    UnitLabIedModelDataSet data_sets[2U];
    uint8_t wire_bytes[256U];
    uint8_t response_bytes[4096U];
    const uint8_t request_payload[] = {
        0x30U, 0x0CU,
        0xA0U, 0x03U, 0x02U, 0x01U, 0x02U,
        0xA1U, 0x05U, 0x81U, 0x03U, 'L', 'D', '0'
    };
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(data_sets, 0, sizeof(data_sets));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "GGIO1");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "GGIO1");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsWire");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 2U;
    plan.logical_nodes = logical_nodes;
    plan.data_set_count = 2U;
    plan.data_sets = data_sets;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(build_get_name_list_request_association_bytes(request_payload, sizeof(request_payload), 61U, wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 2U);
    assert(server_runtime.pending_request.browse_object_scope == 1U);
    assert(strcmp(server_runtime.pending_request.browse_domain_id, "LD0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        size_t ds_wire_offset = 0U;
        size_t ds_events_offset = 0U;

        assert(find_bytes_offset(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"GGIO1$dsWire", strlen("GGIO1$dsWire"), &ds_wire_offset) == 1);
        assert(find_bytes_offset(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"LLN0$dsEvents", strlen("LLN0$dsEvents"), &ds_events_offset) == 1);
        assert(ds_wire_offset < ds_events_offset);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"LLN0$dsUpdates", strlen("LLN0$dsUpdates")) == 0);
    }
}

static void test_server_runtime_apply_iedscout_get_name_list_request_matches_golden_capture(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    uint8_t response_bytes[4096U];
    size_t response_length = 0U;
    size_t consumed_length = 0U;
    static const uint8_t request_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x24U, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U, 0x61U, 0x17U, 0x30U, 0x15U, 0x02U, 0x01U, 0x03U, 0xA0U, 0x10U, 0xA0U, 0x0EU, 0x02U, 0x01U, 0x01U, 0xA1U, 0x09U, 0xA0U, 0x03U, 0x80U, 0x01U, 0x09U, 0xA1U, 0x02U, 0x80U, 0x00U
    };
    const uint8_t expected_identifier[] = { 'S', 'a', 'm', 'p', 'l', 'e', 'I', 'E', 'D', 'D', 'e', 'v', 'i', 'c', 'e', '1' };

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "SampleIEDDevice1");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, sizeof(request_bytes), &consumed_length, &operation_result));
    assert(consumed_length == sizeof(request_bytes));
    assert(operation_result.ok == 1);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 9U);
    assert(server_runtime.pending_request.browse_object_scope == 0U);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(response_length > 0U);
    assert(contains_bytes(response_bytes, response_length, expected_identifier, sizeof(expected_identifier)) == 1);
}



static void test_server_runtime_fixture_domain_get_name_list_uses_mms_domain(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedFixtureSignal signals[1U] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1U] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1U] = {
        {
            .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
            .logical_device_inst = "LD0",
            .logical_node_name = "LLN0",
            .report_control_name = "brcbEvents",
            .report_kind = "buffered",
            .rpt_id = "IED1LD0/LLN0.BR.Events",
            .data_set_ref = "IED1/AP1/LD0/LLN0.dsEvents",
            .conf_rev = "11",
            .indexed_known = 1,
            .indexed = 0,
            .buffer_time_ms_known = 1,
            .buffer_time_ms = 100,
            .integrity_period_ms_known = 1,
            .integrity_period_ms = 1000,
        },
    };
    UnitLabIedFixtureModel fixture = {
        .device_count = 1U,
        .ied_name = "IED1",
        .access_point_name = "AP1",
        .data_set_count = 1U,
        .data_sets = data_sets,
        .report_count = 1U,
        .reports = reports,
        .signal_count = 1U,
    };
    UnitLabIedModelPlan plan;
    char error[256U];
    uint8_t response_bytes[4096U];
    size_t response_length = 0U;
    size_t consumed_length = 0U;
    static const uint8_t request_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x24U, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U, 0x61U, 0x17U, 0x30U, 0x15U, 0x02U, 0x01U, 0x03U, 0xA0U, 0x10U, 0xA0U, 0x0EU, 0x02U, 0x01U, 0x01U, 0xA1U, 0x09U, 0xA0U, 0x03U, 0x80U, 0x01U, 0x09U, 0xA1U, 0x02U, 0x80U, 0x00U
    };
    const uint8_t expected_identifier[] = { 'I', 'E', 'D', '1', 'L', 'D', '0' };

    assert(unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error)) == 1);
    assert(strcmp(plan.logical_devices[0].inst, "IED1LD0") == 0);

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, sizeof(request_bytes), &consumed_length, &operation_result));
    assert(consumed_length == sizeof(request_bytes));
    assert(operation_result.ok == 1);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 9U);
    assert(server_runtime.pending_request.browse_object_scope == 0U);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(response_length > 0U);
    assert(contains_bytes(response_bytes, response_length, expected_identifier, sizeof(expected_identifier)) == 1);

    unitlab_free_ied_model_plan(&plan);
}

static void test_server_runtime_apply_iedscout_logical_node_directory_request_class_one_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[2U];
    UnitLabIedModelSignal signals[2U];
    uint8_t wire_bytes[512U];
    uint8_t response_bytes[2048U];
    uint8_t scratch[512U];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(signals, 0, sizeof(signals));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "XCBR1");
    snprintf(signals[0].logical_device_inst, sizeof(signals[0].logical_device_inst), "%s", "LD0");
    snprintf(signals[0].logical_node_name, sizeof(signals[0].logical_node_name), "%s", "XCBR1");
    snprintf(signals[0].data_object_name, sizeof(signals[0].data_object_name), "%s", "Pos");
    snprintf(signals[1].logical_device_inst, sizeof(signals[1].logical_device_inst), "%s", "LD0");
    snprintf(signals[1].logical_node_name, sizeof(signals[1].logical_node_name), "%s", "XCBR1");
    snprintf(signals[1].data_object_name, sizeof(signals[1].data_object_name), "%s", "Loc");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 2U;
    plan.logical_nodes = logical_nodes;
    plan.signal_count = 2U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(unitlab_mms_build_get_name_list_request_frame(1U, 1U, "LD0", "LLN0", 7U, scratch, sizeof(scratch), wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 1U);
    assert(server_runtime.pending_request.browse_object_scope == 1U);
    assert(strcmp(server_runtime.pending_request.browse_domain_id, "LD0") == 0);
    assert(strcmp(server_runtime.pending_request.browse_continue_after, "LLN0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Mod", strlen("Mod")) == 1);
    }
}

static void test_server_runtime_unsupported_confirmed_service_builds_confirmed_error(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabMmsPdu request_pdu;
    UnitLabMmsAssociationFrame response_frame;
    UnitLabMmsPdu response_pdu;
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[512U];
    const uint8_t unsupported_confirmed_request[] = { 0x02U, 0x01U, 0x23U, 0xA7U, 0x00U };
    size_t request_length = 0U;
    size_t response_length = 0U;
    size_t consumed_length = 0U;
    size_t response_consumed_length = 0U;
    size_t pdu_consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    unitlab_mms_pdu_init(&request_pdu);
    request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    request_pdu.has_invoke_id = 1;
    request_pdu.invoke_id = 35U;
    request_pdu.has_service = 1;
    request_pdu.service_kind = UNITLAB_MMS_SERVICE_RAW;
    request_pdu.pdu_bytes = unsupported_confirmed_request;
    request_pdu.pdu_length = sizeof(unsupported_confirmed_request);
    assert(unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result) == 0);
    assert(operation_result.ok == 0);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
    assert(server_runtime.last_wire_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
    assert(server_runtime.last_wire_pdu.invoke_id == 35U);
    assert(server_runtime.last_wire_pdu.service_kind == UNITLAB_MMS_SERVICE_RAW);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_error_bytes(&server_runtime, server_runtime.last_wire_pdu.invoke_id, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);

    unitlab_mms_association_frame_init(&response_frame);
    assert(unitlab_mms_association_frame_decode(&response_frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    unitlab_mms_pdu_init(&response_pdu);
    assert(unitlab_mms_pdu_decode(&response_pdu, response_frame.presentation.payload_bytes, response_frame.presentation.payload_length, &pdu_consumed_length, &diagnostic));
    assert(pdu_consumed_length == response_frame.presentation.payload_length);
    assert(response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_ERROR);
    assert(response_pdu.has_invoke_id == 1);
    assert(response_pdu.invoke_id == 35U);
    assert(response_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(response_pdu.service_tag.constructed == 1);
    assert(response_pdu.service_tag.tag_number == 2U);
}

static void test_server_runtime_build_confirmed_error_bytes_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[4U];
    UnitLabIedModelDataSet data_sets[4U];
    uint8_t request_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x2AU, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U,
        0x61U, 0x1DU, 0x30U, 0x1BU, 0x02U, 0x01U, 0x03U, 0xA0U, 0x16U, 0xA0U, 0x14U,
        0x02U, 0x01U, 0x03U, 0xA6U, 0x0FU, 0xA0U, 0x0DU, 0xA1U, 0x0BU, 0x1AU, 0x03U,
        'L', 'D', '0', 0x1AU, 0x04U, 'L', 'L', 'N', '0'
    };
    uint8_t response_bytes[4096U];
    size_t request_length = sizeof(request_bytes);
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(data_sets, 0, sizeof(data_sets));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "XCBR1");
    snprintf(logical_nodes[2].logical_device_inst, sizeof(logical_nodes[2].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[2].name, sizeof(logical_nodes[2].name), "%s", "PGGIO1");
    snprintf(logical_nodes[3].logical_device_inst, sizeof(logical_nodes[3].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[3].name, sizeof(logical_nodes[3].name), "%s", "GGIO1");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "XCBR1");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsEvents");
    snprintf(data_sets[2].logical_device_inst, sizeof(data_sets[2].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[2].logical_node_name, sizeof(data_sets[2].logical_node_name), "%s", "PGGIO1");
    snprintf(data_sets[2].name, sizeof(data_sets[2].name), "%s", "dsEvents");
    snprintf(data_sets[3].logical_device_inst, sizeof(data_sets[3].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[3].logical_node_name, sizeof(data_sets[3].logical_node_name), "%s", "GGIO1");
    snprintf(data_sets[3].name, sizeof(data_sets[3].name), "%s", "dsWire");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 4U;
    plan.logical_nodes = logical_nodes;
    plan.data_set_count = 4U;
    plan.data_sets = data_sets;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(consumed_length == request_length);
    assert(operation_result.ok == 1);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES);
    assert(strcmp(server_runtime.pending_request.object_reference, "LD0.LLN0") == 0);
    assert(strcmp(server_runtime.pending_request.attribute_reference, "LLN0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;
        UnitLabMmsBerElement invoke_element;
        UnitLabMmsBerElement service_element;
        UnitLabMmsBerElement service_field_element;
        UnitLabMmsBerElement type_spec_outer_element;
        UnitLabMmsBerElement type_spec_inner_element;
        UnitLabMmsBerElement components_wrapper_element;
        UnitLabMmsBerElement component_element;
        UnitLabMmsBerElement component_name_element;
        UnitLabMmsBerElement component_type_element;
        UnitLabMmsPdu decoded_pdu;
        size_t frame_consumed_length = 0U;
        size_t pdu_consumed_length = 0U;
        size_t element_consumed_length = 0U;
        size_t service_offset = 0U;
        size_t component_offset = 0U;
        size_t component_count = 0U;
        size_t component_name_consumed_length = 0U;
        size_t component_type_consumed_length = 0U;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &frame_consumed_length, &diagnostic));
        assert(frame_consumed_length == response_length);
        assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        unitlab_mms_pdu_init(&decoded_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_pdu, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &pdu_consumed_length, &diagnostic));
        assert(pdu_consumed_length == fixture.presentation.payload_length);
        assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_pdu.has_invoke_id == 1);
        assert(decoded_pdu.invoke_id == 3U);
        assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES);
        assert(decoded_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
        assert(decoded_pdu.service_tag.constructed == 1);
        assert(decoded_pdu.service_tag.tag_number == 6U);

        unitlab_mms_ber_element_init(&invoke_element);
        assert(unitlab_mms_ber_read(&invoke_element, decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, &element_consumed_length, &diagnostic));
        assert_ber_tag(&invoke_element, UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 0, 2U);

        unitlab_mms_ber_element_init(&service_element);
        assert(unitlab_mms_ber_read(&service_element, &decoded_pdu.pdu_bytes[element_consumed_length], decoded_pdu.pdu_length - element_consumed_length, &service_offset, &diagnostic));
        assert_ber_tag(&service_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 6U);

        unitlab_mms_ber_element_init(&service_field_element);
        assert(unitlab_mms_ber_read(&service_field_element, service_element.value_bytes, service_element.value_length, &element_consumed_length, &diagnostic));
        assert_ber_tag(&service_field_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 0U);
        assert(service_field_element.value_length == 1U);
        assert(service_field_element.value_bytes[0] == 0x00U);

        unitlab_mms_ber_element_init(&type_spec_outer_element);
        assert(unitlab_mms_ber_read(&type_spec_outer_element, &service_element.value_bytes[element_consumed_length], service_element.value_length - element_consumed_length, &service_offset, &diagnostic));
        assert_ber_tag(&type_spec_outer_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 2U);

        unitlab_mms_ber_element_init(&type_spec_inner_element);
        assert(unitlab_mms_ber_read(&type_spec_inner_element, type_spec_outer_element.value_bytes, type_spec_outer_element.value_length, &element_consumed_length, &diagnostic));
        assert_ber_tag(&type_spec_inner_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 2U);

        unitlab_mms_ber_element_init(&components_wrapper_element);
        assert(unitlab_mms_ber_read(&components_wrapper_element, type_spec_inner_element.value_bytes, type_spec_inner_element.value_length, &element_consumed_length, &diagnostic));
        assert_ber_tag(&components_wrapper_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U);

        for (component_offset = 0U; component_offset < components_wrapper_element.value_length; ) {
            size_t component_consumed_length = 0U;
            size_t component_inner_offset = 0U;

            unitlab_mms_ber_element_init(&component_element);
            assert(unitlab_mms_ber_read(&component_element, &components_wrapper_element.value_bytes[component_offset], components_wrapper_element.value_length - component_offset, &component_consumed_length, &diagnostic));
            assert_ber_tag(&component_element, UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL, 1, 16U);
            assert(components_wrapper_element.value_bytes[component_offset] == 0x30U);
            assert(component_element.value_bytes[0] == 0x80U);

            unitlab_mms_ber_element_init(&component_name_element);
            assert(unitlab_mms_ber_read(&component_name_element, component_element.value_bytes, component_element.value_length, &component_name_consumed_length, &diagnostic));
            assert_ber_tag(&component_name_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 0U);

            component_inner_offset += component_name_consumed_length;
            unitlab_mms_ber_element_init(&component_type_element);
            assert(unitlab_mms_ber_read(&component_type_element, &component_element.value_bytes[component_inner_offset], component_element.value_length - component_inner_offset, &component_type_consumed_length, &diagnostic));
            assert(component_element.value_bytes[component_name_consumed_length] == 0xA1U);
            assert_ber_tag(&component_type_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U);
            assert(component_type_element.value_length > 0U);
            component_inner_offset += component_type_consumed_length;
            assert(component_inner_offset == component_element.value_length);

            component_offset += component_consumed_length;
            component_count++;
        }
        assert(component_count == 0U);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Mod", strlen("Mod")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Beh", strlen("Beh")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Health", strlen("Health")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"EX", strlen("EX")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"ldNs", strlen("ldNs")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"lnNs", strlen("lnNs")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"cdcNs", strlen("cdcNs")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"dataNs", strlen("dataNs")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"brcbEvents", strlen("brcbEvents")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"RptID", strlen("RptID")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"DatSet", strlen("DatSet")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"ResvTms", strlen("ResvTms")) == 0);
        assert(component_offset == components_wrapper_element.value_length);
    }
}


static void test_server_runtime_apply_iedscout_vmd_directory_request_scope_zero_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[4U];
    UnitLabIedModelDataSet data_sets[4U];
    uint8_t response_bytes[2048U];
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(data_sets, 0, sizeof(data_sets));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "XCBR1");
    snprintf(logical_nodes[2].logical_device_inst, sizeof(logical_nodes[2].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[2].name, sizeof(logical_nodes[2].name), "%s", "PGGIO1");
    snprintf(logical_nodes[3].logical_device_inst, sizeof(logical_nodes[3].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[3].name, sizeof(logical_nodes[3].name), "%s", "GGIO1");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "XCBR1");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsEvents");
    snprintf(data_sets[2].logical_device_inst, sizeof(data_sets[2].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[2].logical_node_name, sizeof(data_sets[2].logical_node_name), "%s", "PGGIO1");
    snprintf(data_sets[2].name, sizeof(data_sets[2].name), "%s", "dsEvents");
    snprintf(data_sets[3].logical_device_inst, sizeof(data_sets[3].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[3].logical_node_name, sizeof(data_sets[3].logical_node_name), "%s", "GGIO1");
    snprintf(data_sets[3].name, sizeof(data_sets[3].name), "%s", "dsWire");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 4U;
    plan.logical_nodes = logical_nodes;
    plan.data_set_count = 4U;
    plan.data_sets = data_sets;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_NAME_LIST, 7U, 1U, 1000U, 100U, &diagnostic) == 1);
    server_runtime.pending_request.browse_object_class = 2U;
    server_runtime.pending_request.browse_object_scope = 0U;
    server_runtime.pending_request.browse_domain_id[0] = '\0';
    server_runtime.pending_request.browse_continue_after[0] = '\0';

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"dsEvents", strlen("dsEvents")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"dsWire", strlen("dsWire")) == 0);
    }
}

static void test_server_runtime_apply_iedscout_aa_specific_directory_request_scope_two_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelDataSet data_sets[2U];
    uint8_t response_bytes[2048U];
    size_t response_length = 0U;
    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(data_sets, 0, sizeof(data_sets));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsWire");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.data_set_count = 2U;
    plan.data_sets = data_sets;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_NAME_LIST, 8U, 1U, 1000U, 100U, &diagnostic) == 1);
    server_runtime.pending_request.browse_object_class = 2U;
    server_runtime.pending_request.browse_object_scope = 2U;
    server_runtime.pending_request.browse_domain_id[0] = '\0';
    server_runtime.pending_request.browse_continue_after[0] = '\0';

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;
        UnitLabMmsPdu decoded_pdu;
        size_t response_frame_consumed_length = 0U;
        size_t response_pdu_consumed_length = 0U;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_frame_consumed_length, &diagnostic));
        assert(response_frame_consumed_length == response_length);
        unitlab_mms_pdu_init(&decoded_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_pdu, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &response_pdu_consumed_length, &diagnostic));
        assert(response_pdu_consumed_length == fixture.presentation.payload_length);
        assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_pdu.has_invoke_id == 1);
        assert(decoded_pdu.invoke_id == 8U);
        assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_GET_NAME_LIST);
        assert(decoded_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
        assert(decoded_pdu.service_tag.constructed == 1);
        assert(decoded_pdu.service_tag.tag_number == 1U);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"dsEvents", strlen("dsEvents")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"dsWire", strlen("dsWire")) == 0);
    }
}

static void test_server_runtime_apply_iedscout_vmd_get_variable_access_attributes_request_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[4U];
    UnitLabIedModelDataSet data_sets[4U];
    uint8_t response_bytes[2048U];
    const uint8_t request_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x2AU, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U,
        0x61U, 0x1DU, 0x30U, 0x1BU, 0x02U, 0x01U, 0x03U, 0xA0U, 0x16U, 0xA0U, 0x14U,
        0x02U, 0x01U, 0x03U, 0xA6U, 0x0FU, 0xA0U, 0x0DU, 0xA1U, 0x0BU, 0x1AU, 0x03U,
        'L', 'D', '0', 0x1AU, 0x04U, 'L', 'L', 'N', '0'
    };
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(data_sets, 0, sizeof(data_sets));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "XCBR1");
    snprintf(logical_nodes[2].logical_device_inst, sizeof(logical_nodes[2].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[2].name, sizeof(logical_nodes[2].name), "%s", "PGGIO1");
    snprintf(logical_nodes[3].logical_device_inst, sizeof(logical_nodes[3].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[3].name, sizeof(logical_nodes[3].name), "%s", "GGIO1");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "XCBR1");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsEvents");
    snprintf(data_sets[2].logical_device_inst, sizeof(data_sets[2].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[2].logical_node_name, sizeof(data_sets[2].logical_node_name), "%s", "PGGIO1");
    snprintf(data_sets[2].name, sizeof(data_sets[2].name), "%s", "dsEvents");
    snprintf(data_sets[3].logical_device_inst, sizeof(data_sets[3].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[3].logical_node_name, sizeof(data_sets[3].logical_node_name), "%s", "GGIO1");
    snprintf(data_sets[3].name, sizeof(data_sets[3].name), "%s", "dsWire");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 4U;
    plan.logical_nodes = logical_nodes;
    plan.data_set_count = 4U;
    plan.data_sets = data_sets;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, sizeof(request_bytes), &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(request_bytes));
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES);
    assert(strcmp(server_runtime.pending_request.object_reference, "LD0.LLN0") == 0);
    assert(strcmp(server_runtime.pending_request.attribute_reference, "LLN0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Mod", strlen("Mod")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Beh", strlen("Beh")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Health", strlen("Health")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"CF", strlen("CF")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"DC", strlen("DC")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"BR", strlen("BR")) == 0);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"EX", strlen("EX")) == 0);
    }
}

static void test_server_runtime_apply_iedscout_logical_node_directory_request_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[1U];
    uint8_t wire_bytes[512U];
    uint8_t response_bytes[2048U];
    const uint8_t request_payload[] = {
        0x30U, 0x1BU,
        0x02U, 0x01U, 0x03U,
        0xA0U, 0x16U,
        0xA0U, 0x14U,
        0x02U, 0x01U, 0x03U,
        0xA6U, 0x0FU,
        0xA0U, 0x0DU,
        0xA1U, 0x0BU,
        0x1AU, 0x03U, 'L', 'D', '0',
        0x1AU, 0x04U, 'L', 'L', 'N', '0'
    };
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 1U;
    plan.logical_nodes = logical_nodes;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(build_get_name_list_request_association_bytes(request_payload, sizeof(request_payload), 3U, wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 3U);
    assert(server_runtime.pending_request.browse_object_scope == 1U);
    assert(strcmp(server_runtime.pending_request.browse_domain_id, "LD0") == 0);
    assert(strcmp(server_runtime.pending_request.browse_continue_after, "LLN0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Mod", strlen("Mod")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Beh", strlen("Beh")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Health", strlen("Health")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"CF", strlen("CF")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"DC", strlen("DC")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"BR", strlen("BR")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"EX", strlen("EX")) == 1);
    }
}


static void test_server_runtime_apply_named_variable_list_attributes_request_and_build_response_roundtrips(void)
{
    struct {
        const uint8_t* request_inner_bytes;
        size_t request_inner_length;
        uint32_t invoke_id;
        const char* object_reference;
        const char* expected_member_token_0;
        const char* expected_member_token_1;
        const char* expected_member_token_2;
        size_t expected_member_token_count;
    } cases[] = {
        { (const uint8_t[]){ 0x02U, 0x01U, 0x0BU, 0xACU, 0x0AU, 0x80U, 0x08U, 'd', 's', 'E', 'v', 'e', 'n', 't', 's' }, 15U, 11U, "dsEvents", "LLN0$ST$Mod", "LLN0$ST$Beh", NULL, 2U },
        { (const uint8_t[]){ 0x02U, 0x01U, 0x0EU, 0xACU, 0x15U, 0xA1U, 0x13U, 0x1AU, 0x03U, 'L', 'D', '0', 0x1AU, 0x0CU, 'G', 'G', 'I', 'O', '1', '$', 'd', 's', 'W', 'i', 'r', 'e' }, 26U, 14U, "LD0/GGIO1$dsWire", "GGIO1$ST$Ind1", NULL, NULL, 1U },
        { (const uint8_t[]){ 0x02U, 0x01U, 0x0FU, 0xACU, 0x17U, 0xA1U, 0x15U, 0x1AU, 0x03U, 'L', 'D', '0', 0x1AU, 0x0EU, 'L', 'L', 'N', '0', '$', 'M', 'E', 'A', 'S', '_', 'R', 'C', 'B', '1' }, 28U, 15U, "LD0/LLN0$MEAS_RCB1", "MMXU1$MX$A$phsA$cVal$mag$f", NULL, NULL, 1U },
    };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelDataSet data_sets[3U];
    UnitLabIedModelReportControl reports[1U];
    UnitLabIedModelSignal signals[4U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[4096U];
    uint8_t scratch[1024U];
    size_t request_length = 0U;
    size_t response_length = 0U;
    size_t consumed_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(data_sets, 0, sizeof(data_sets));
    memset(reports, 0, sizeof(reports));
    memset(signals, 0, sizeof(signals));
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    data_sets[0].first_signal_index = 0U;
    data_sets[0].member_count = 2U;
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "GGIO1");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsWire");
    data_sets[1].first_signal_index = 2U;
    data_sets[1].member_count = 1U;
    snprintf(data_sets[2].logical_device_inst, sizeof(data_sets[2].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[2].logical_node_name, sizeof(data_sets[2].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[2].name, sizeof(data_sets[2].name), "%s", "dsMeas");
    data_sets[2].first_signal_index = 3U;
    data_sets[2].member_count = 1U;
    snprintf(reports[0].logical_device_inst, sizeof(reports[0].logical_device_inst), "%s", "LD0");
    snprintf(reports[0].logical_node_name, sizeof(reports[0].logical_node_name), "%s", "LLN0");
    snprintf(reports[0].name, sizeof(reports[0].name), "%s", "MEAS_RCB1");
    reports[0].data_set_index = 2U;
    snprintf(signals[0].data_set_entry_variable, sizeof(signals[0].data_set_entry_variable), "%s", "LD0/LLN0$ST$Mod");
    snprintf(signals[1].data_set_entry_variable, sizeof(signals[1].data_set_entry_variable), "%s", "LD0/LLN0$ST$Beh");
    snprintf(signals[2].data_set_entry_variable, sizeof(signals[2].data_set_entry_variable), "%s", "LD0/GGIO1$ST$Ind1");
    snprintf(signals[3].data_set_entry_variable, sizeof(signals[3].data_set_entry_variable), "%s", "LD0/MMXU1$MX$A$phsA$cVal$mag$f");
    plan.data_set_count = 3U;
    plan.data_sets = data_sets;
    plan.report_count = 1U;
    plan.reports = reports;
    plan.signal_count = 4U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++) {
        UnitLabMmsPdu request_pdu;
        UnitLabMmsAssociationFrame frame;
        UnitLabMmsPdu response_pdu;
        UnitLabMmsOperationResult operation_result_local;

        unitlab_mms_pdu_init(&request_pdu);
        request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
        request_pdu.has_invoke_id = 1;
        request_pdu.invoke_id = cases[index].invoke_id;
        request_pdu.has_service = 1;
        request_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES;
        request_pdu.pdu_bytes = cases[index].request_inner_bytes;
        request_pdu.pdu_length = cases[index].request_inner_length;

        assert(unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
        unitlab_mms_operation_result_init(&operation_result_local);
        assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result_local));
        assert(operation_result_local.ok == 1);
        assert(consumed_length == request_length);
        assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAMED_VARIABLE_LIST_ATTRIBUTES);
        assert(strcmp(server_runtime.pending_request.object_reference, cases[index].object_reference) == 0);

        unitlab_mms_diagnostic_clear(&diagnostic);
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
        assert(response_length > 0U);

        unitlab_mms_association_frame_init(&frame);
        assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        unitlab_mms_pdu_init(&response_pdu);
        assert(unitlab_mms_pdu_decode(&response_pdu, frame.presentation.payload_bytes, frame.presentation.payload_length, &consumed_length, &diagnostic));
        assert(consumed_length == frame.presentation.payload_length);
        assert(response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(response_pdu.has_invoke_id == 1);
        assert(response_pdu.invoke_id == cases[index].invoke_id);
        assert(response_pdu.service_kind == UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES);
        assert(response_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
        assert(response_pdu.service_tag.constructed == 1);
        assert(response_pdu.service_tag.tag_number == 12U);
        assert(response_pdu.service_length > 0U);

        {
            UnitLabMmsSemanticResult semantic_result;
            UnitLabMmsDecodeDiagnostic bridge_diagnostic;

            unitlab_mms_semantic_result_init(&semantic_result);
            unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);
            assert(unitlab_mms_semantic_result_from_wire_pdu(&semantic_result, &response_pdu, &bridge_diagnostic) == 1);
            assert(semantic_result.ok == 1);
            assert(semantic_result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
            assert(semantic_result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_RESPONSE);
            assert(semantic_result.pdu.invoke_id == cases[index].invoke_id);
            assert(bridge_diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
        }

        assert_named_variable_list_attributes_response_shape(
            &response_pdu,
            cases[index].expected_member_token_0,
            cases[index].expected_member_token_1,
            cases[index].expected_member_token_2,
            cases[index].expected_member_token_count);
        assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)cases[index].expected_member_token_0, strlen(cases[index].expected_member_token_0)) == 1);
        if (cases[index].expected_member_token_count > 1U) {
            assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)cases[index].expected_member_token_1, strlen(cases[index].expected_member_token_1)) == 1);
        }
        unitlab_mms_pending_request_init(&server_runtime.pending_request);
    }
}


static void test_server_runtime_apply_named_variable_list_attributes_request_and_build_response_matches_live_fixture_style(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelDataSet data_sets[1U];
    UnitLabIedModelSignal signals[2U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[4096U];
    uint8_t scratch[1024U];
    size_t request_length = 0U;
    size_t response_length = 0U;
    size_t consumed_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(data_sets, 0, sizeof(data_sets));
    memset(signals, 0, sizeof(signals));
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    data_sets[0].first_signal_index = 0U;
    data_sets[0].member_count = 2U;
    snprintf(signals[0].logical_device_inst, sizeof(signals[0].logical_device_inst), "%s", "LD0");
    snprintf(signals[0].logical_node_name, sizeof(signals[0].logical_node_name), "%s", "XCBR1");
    snprintf(signals[0].object_reference, sizeof(signals[0].object_reference), "%s", "Pos.stVal");
    snprintf(signals[0].data_set_entry_variable, sizeof(signals[0].data_set_entry_variable), "%s", "LD0/XCBR1$ST$Pos$stVal");
    snprintf(signals[0].fc, sizeof(signals[0].fc), "%s", "ST");
    snprintf(signals[1].logical_device_inst, sizeof(signals[1].logical_device_inst), "%s", "LD0");
    snprintf(signals[1].logical_node_name, sizeof(signals[1].logical_node_name), "%s", "PGGIO1");
    snprintf(signals[1].object_reference, sizeof(signals[1].object_reference), "%s", "Ind1.stVal");
    snprintf(signals[1].data_set_entry_variable, sizeof(signals[1].data_set_entry_variable), "%s", "LD0/PGGIO1$ST$Ind1$stVal");
    snprintf(signals[1].fc, sizeof(signals[1].fc), "%s", "ST");
    plan.data_set_count = 1U;
    plan.data_sets = data_sets;
    plan.signal_count = 2U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        UnitLabMmsPdu request_pdu;
        UnitLabMmsAssociationFrame frame;
        UnitLabMmsPdu response_pdu;
        UnitLabMmsSemanticResult semantic_result;
        UnitLabMmsDecodeDiagnostic bridge_diagnostic;

        unitlab_mms_pdu_init(&request_pdu);
        request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
        request_pdu.has_invoke_id = 1;
        request_pdu.invoke_id = 13U;
        request_pdu.has_service = 1;
        request_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES;
        request_pdu.pdu_bytes = (const uint8_t[]){ 0x02U, 0x01U, 0x0DU, 0xACU, 0x16U, 0xA1U, 0x14U, 0x1AU, 0x03U, 'L', 'D', '0', 0x1AU, 0x0DU, 'L', 'L', 'N', '0', '$', 'd', 's', 'E', 'v', 'e', 'n', 't', 's' };
        request_pdu.pdu_length = 27U;

        assert(unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
        unitlab_mms_operation_result_init(&operation_result);
        assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
        assert(operation_result.ok == 1);
        assert(consumed_length == request_length);
        assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAMED_VARIABLE_LIST_ATTRIBUTES);
        assert(strcmp(server_runtime.pending_request.object_reference, "LD0/LLN0$dsEvents") == 0);

        unitlab_mms_diagnostic_clear(&diagnostic);
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
        assert(response_length > 0U);

        unitlab_mms_association_frame_init(&frame);
        assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        unitlab_mms_pdu_init(&response_pdu);
        assert(unitlab_mms_pdu_decode(&response_pdu, frame.presentation.payload_bytes, frame.presentation.payload_length, &consumed_length, &diagnostic));
        assert(consumed_length == frame.presentation.payload_length);
        assert(response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(response_pdu.has_invoke_id == 1);
        assert(response_pdu.invoke_id == 13U);
        assert(response_pdu.service_kind == UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES);
        assert_named_variable_list_attributes_response_shape(&response_pdu, "XCBR1$ST$Pos$stVal", "PGGIO1$ST$Ind1$stVal", NULL, 2U);
        assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"XCBR1$ST$Pos$stVal", strlen("XCBR1$ST$Pos$stVal")) == 1);
        assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"PGGIO1$ST$Ind1$stVal", strlen("PGGIO1$ST$Ind1$stVal")) == 1);
        unitlab_mms_semantic_result_init(&semantic_result);
        unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);
        assert(unitlab_mms_semantic_result_from_wire_pdu(&semantic_result, &response_pdu, &bridge_diagnostic) == 1);
        assert(semantic_result.ok == 1);
        assert(semantic_result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
        assert(semantic_result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_RESPONSE);
        assert(bridge_diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
    }
}



static void test_server_runtime_named_variable_list_attributes_returns_expanded_leaf_members(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    UnitLabIedModelDataSet data_sets[1U];
    UnitLabIedModelSignal signals[3U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[4096U];
    uint8_t scratch[1024U];
    size_t request_length = 0U;
    size_t response_length = 0U;
    size_t consumed_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(data_sets, 0, sizeof(data_sets));
    memset(signals, 0, sizeof(signals));
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    data_sets[0].first_signal_index = 0U;
    data_sets[0].member_count = 3U;
    for (size_t index = 0U; index < 3U; index++) {
        snprintf(signals[index].logical_device_inst, sizeof(signals[index].logical_device_inst), "%s", "LD0");
        snprintf(signals[index].logical_node_name, sizeof(signals[index].logical_node_name), "%s", "PGGIO1");
        snprintf(signals[index].data_object_name, sizeof(signals[index].data_object_name), "%s", "Ind1");
        snprintf(signals[index].fc, sizeof(signals[index].fc), "%s", "ST");
        signals[index].data_set_entry_component_known = 1;
    }
    snprintf(signals[0].data_set_entry_component, sizeof(signals[0].data_set_entry_component), "%s", "stVal");
    snprintf(signals[0].data_set_entry_variable, sizeof(signals[0].data_set_entry_variable), "%s", "LD0/PGGIO1$ST$Ind1$stVal");
    snprintf(signals[1].data_set_entry_component, sizeof(signals[1].data_set_entry_component), "%s", "q");
    snprintf(signals[1].data_set_entry_variable, sizeof(signals[1].data_set_entry_variable), "%s", "LD0/PGGIO1$ST$Ind1$q");
    snprintf(signals[2].data_set_entry_component, sizeof(signals[2].data_set_entry_component), "%s", "t");
    snprintf(signals[2].data_set_entry_variable, sizeof(signals[2].data_set_entry_variable), "%s", "LD0/PGGIO1$ST$Ind1$t");
    plan.data_set_count = 1U;
    plan.data_sets = data_sets;
    plan.signal_count = 3U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    {
        UnitLabMmsPdu request_pdu;
        UnitLabMmsAssociationFrame frame;
        UnitLabMmsPdu response_pdu;

        unitlab_mms_pdu_init(&request_pdu);
        request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
        request_pdu.has_invoke_id = 1;
        request_pdu.invoke_id = 14U;
        request_pdu.has_service = 1;
        request_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES;
        request_pdu.pdu_bytes = (const uint8_t[]){ 0x02U, 0x01U, 0x0EU, 0xACU, 0x16U, 0xA1U, 0x14U, 0x1AU, 0x03U, 'L', 'D', '0', 0x1AU, 0x0DU, 'L', 'L', 'N', '0', '$', 'd', 's', 'E', 'v', 'e', 'n', 't', 's' };
        request_pdu.pdu_length = 27U;

        assert(unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
        unitlab_mms_operation_result_init(&operation_result);
        assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
        assert(operation_result.ok == 1);
        assert(consumed_length == request_length);

        unitlab_mms_diagnostic_clear(&diagnostic);
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
        assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
        assert(response_length > 0U);

        unitlab_mms_association_frame_init(&frame);
        assert(unitlab_mms_association_frame_decode(&frame, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        unitlab_mms_pdu_init(&response_pdu);
        assert(unitlab_mms_pdu_decode(&response_pdu, frame.presentation.payload_bytes, frame.presentation.payload_length, &consumed_length, &diagnostic));
        assert(consumed_length == frame.presentation.payload_length);
        assert(response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(response_pdu.service_kind == UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES);
        assert_named_variable_list_attributes_response_shape(&response_pdu, "PGGIO1$ST$Ind1$stVal", "PGGIO1$ST$Ind1$q", "PGGIO1$ST$Ind1$t", 3U);
        assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"PGGIO1$ST$Ind1$stVal", strlen("PGGIO1$ST$Ind1$stVal")) == 1);
        assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"PGGIO1$ST$Ind1$q", strlen("PGGIO1$ST$Ind1$q")) == 1);
        assert(contains_bytes(frame.presentation.payload_bytes, frame.presentation.payload_length, (const uint8_t*)"PGGIO1$ST$Ind1$t", strlen("PGGIO1$ST$Ind1$t")) == 1);
    }
}


static void test_server_runtime_gi_report_handles_large_model_dataset(void)
{
    enum { signal_count = 120U };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    UnitLabIedModelDataSet data_sets[1U];
    UnitLabIedModelReportControl reports[1U];
    UnitLabIedModelSignal signals[signal_count];
    UnitLabMmsAssociationFrame report_frame;
    UnitLabMmsPdu report_pdu;
    uint8_t report_bytes[65535U];
    size_t report_length = 0U;
    size_t consumed_length = 0U;
    static const uint8_t expected_inclusion_prefix[] = {
        0x84U, 0x10U, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
        0xFFU, 0xFFU
    };

    memset(&plan, 0, sizeof(plan));
    memset(data_sets, 0, sizeof(data_sets));
    memset(reports, 0, sizeof(reports));
    memset(signals, 0, sizeof(signals));

    snprintf(data_sets[0].reference, sizeof(data_sets[0].reference), "%s", "KINTE08TDIFF/AP1/KINTE08TDIFFSystem/LLN0.RCB2");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "KINTE08TDIFFSystem");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "RCB2");
    data_sets[0].first_signal_index = 0U;
    data_sets[0].member_count = signal_count;

    snprintf(reports[0].key, sizeof(reports[0].key), "%s", "KINTE08TDIFF/AP1/KINTE08TDIFFSystem/LLN0/brcbB/buffered");
    snprintf(reports[0].logical_device_inst, sizeof(reports[0].logical_device_inst), "%s", "KINTE08TDIFFSystem");
    snprintf(reports[0].logical_node_name, sizeof(reports[0].logical_node_name), "%s", "LLN0");
    snprintf(reports[0].name, sizeof(reports[0].name), "%s", "brcbB");
    snprintf(reports[0].report_kind, sizeof(reports[0].report_kind), "%s", "buffered");
    snprintf(reports[0].rpt_id, sizeof(reports[0].rpt_id), "%s", "KINTE08TDIFFSystem/LLN0.BR.brcbB");
    snprintf(reports[0].data_set_ref, sizeof(reports[0].data_set_ref), "%s", "KINTE08TDIFF/AP1/KINTE08TDIFFSystem/LLN0.RCB2");
    reports[0].is_buffered = 1;
    reports[0].data_set_index = 0U;
    reports[0].conf_rev_known = 1;
    reports[0].conf_rev = 12U;
    reports[0].buffer_time_ms_known = 1;
    reports[0].buffer_time_ms = 100U;
    reports[0].integrity_period_ms_known = 1;
    reports[0].integrity_period_ms = 1000U;
    reports[0].trigger_options_mask = UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED | UNITLAB_IED_MODEL_TRG_OPT_GI;
    reports[0].optional_fields_mask = UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM
        | UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP
        | UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION
        | UNITLAB_IED_MODEL_RPT_OPT_DATA_SET
        | UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE
        | UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW
        | UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID
        | UNITLAB_IED_MODEL_RPT_OPT_CONF_REV;

    for (size_t index = 0U; index < signal_count; index++) {
        snprintf(signals[index].reference, sizeof(signals[index].reference), "KINTE08TDIFFSystem/MMXU%zu.A.phsA.cVal.mag.f[MX]", index + 1U);
        snprintf(signals[index].kind, sizeof(signals[index].kind), "%s", "FCDA");
        signals[index].data_set_index = 0U;
        signals[index].member_index = index;
        snprintf(signals[index].logical_device_inst, sizeof(signals[index].logical_device_inst), "%s", "KINTE08TDIFFSystem");
        snprintf(signals[index].logical_node_name, sizeof(signals[index].logical_node_name), "MMXU%zu", index + 1U);
        snprintf(signals[index].data_object_name, sizeof(signals[index].data_object_name), "%s", "A");
        snprintf(signals[index].data_attribute_path, sizeof(signals[index].data_attribute_path), "%s", "phsA.cVal.mag.f");
        snprintf(signals[index].object_reference, sizeof(signals[index].object_reference), "A.phsA.cVal.mag.f");
        snprintf(signals[index].data_set_entry_variable, sizeof(signals[index].data_set_entry_variable), "KINTE08TDIFFSystem/MMXU%zu$MX$A$phsA$cVal$mag$f", index + 1U);
        snprintf(signals[index].fc, sizeof(signals[index].fc), "%s", "MX");
        signals[index].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER;
        snprintf(signals[index].initial_value, sizeof(signals[index].initial_value), "%zu", index);
    }

    plan.data_set_count = 1U;
    plan.data_sets = data_sets;
    plan.report_count = 1U;
    plan.reports = reports;
    plan.signal_count = signal_count;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;
    server_runtime.pending_gi_report = 1U;
    server_runtime.pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_GI;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(report_length > 4096U);
    assert_information_report_wire_envelope(report_bytes, report_length);

    unitlab_mms_association_frame_init(&report_frame);
    assert(unitlab_mms_association_frame_decode(&report_frame, report_bytes, report_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_length);
    unitlab_mms_pdu_init(&report_pdu);
    assert(unitlab_mms_pdu_decode(&report_pdu, report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, &consumed_length, &diagnostic));
    assert(consumed_length == report_frame.presentation.payload_length);
    assert(report_pdu.kind == UNITLAB_MMS_PDU_UNCONFIRMED);
    assert(report_pdu.service_kind == UNITLAB_MMS_SERVICE_INFORMATION_REPORT);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, expected_inclusion_prefix, sizeof(expected_inclusion_prefix)) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"KINTE08TDIFFSystem/MMXU120$MX$A$phsA$cVal$mag$f", strlen("KINTE08TDIFFSystem/MMXU120$MX$A$phsA$cVal$mag$f")) == 1);
    assert(contains_bytes(report_frame.presentation.payload_bytes, report_frame.presentation.payload_length, (const uint8_t*)"KINTE08TDIFFSystem/LLN0$RCB2", strlen("KINTE08TDIFFSystem/LLN0$RCB2")) == 1);
}

static void test_server_runtime_named_variable_list_attributes_handles_large_model_dataset(void)
{
    enum { signal_count = 120U };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    UnitLabIedModelDataSet data_sets[1U];
    UnitLabIedModelSignal signals[signal_count];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[65535U];
    uint8_t scratch[4096U];
    size_t request_length = 0U;
    size_t response_length = 0U;
    size_t consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(data_sets, 0, sizeof(data_sets));
    memset(signals, 0, sizeof(signals));

    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "KINTE08TDIFFSystem");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "RCB2");
    data_sets[0].first_signal_index = 0U;
    data_sets[0].member_count = signal_count;

    for (size_t index = 0U; index < signal_count; index++) {
        snprintf(signals[index].logical_device_inst, sizeof(signals[index].logical_device_inst), "%s", "KINTE08TDIFFSystem");
        snprintf(signals[index].logical_node_name, sizeof(signals[index].logical_node_name), "MMXU%zu", index + 1U);
        snprintf(signals[index].fc, sizeof(signals[index].fc), "%s", "MX");
        snprintf(
            signals[index].data_set_entry_variable,
            sizeof(signals[index].data_set_entry_variable),
            "KINTE08TDIFFSystem/MMXU%zu$MX$A$phsA$cVal$mag$f",
            index + 1U);
    }

    plan.data_set_count = 1U;
    plan.data_sets = data_sets;
    plan.signal_count = signal_count;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    {
        UnitLabMmsPdu request_pdu;
        static const uint8_t request_inner_bytes[] = {
            0x02U, 0x02U, 0x00U, 0xC4U, 0xACU, 0x21U, 0xA1U, 0x1FU,
            0x1AU, 0x12U, 'K', 'I', 'N', 'T', 'E', '0', '8', 'T', 'D', 'I', 'F', 'F', 'S', 'y', 's', 't', 'e', 'm',
            0x1AU, 0x09U, 'L', 'L', 'N', '0', '$', 'R', 'C', 'B', '2'
        };

        unitlab_mms_pdu_init(&request_pdu);
        request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
        request_pdu.has_invoke_id = 1;
        request_pdu.invoke_id = 196U;
        request_pdu.has_service = 1;
        request_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES;
        request_pdu.pdu_bytes = request_inner_bytes;
        request_pdu.pdu_length = sizeof(request_inner_bytes);
        assert(unitlab_mms_build_wire_frame_from_pdu(&request_pdu, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    }

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAMED_VARIABLE_LIST_ATTRIBUTES);
    assert(strcmp(server_runtime.pending_request.object_reference, "KINTE08TDIFFSystem/LLN0$RCB2") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 4096U);
    assert(contains_bytes(response_bytes, response_length, (const uint8_t*)"MMXU120$MX$A$phsA$cVal$mag$f", strlen("MMXU120$MX$A$phsA$cVal$mag$f")) == 1);
}


static void test_server_runtime_model_urcb_read_uses_unbuffered_rcb_shape(void)
{
    static const uint32_t expected_tags[] = { 10U, 3U, 3U, 10U, 6U, 4U, 6U, 6U, 4U, 6U, 3U };
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedModelPlan model_plan;
    UnitLabIedModelLogicalDevice logical_device;
    UnitLabIedModelDataSet data_set;
    UnitLabIedModelReportControl report;
    UnitLabMmsBerElement rcb_element;
    UnitLabMmsBerElement child_element;
    uint8_t rcb_bytes[1024U];
    size_t rcb_length = 0U;
    size_t consumed_length = 0U;
    size_t child_offset = 0U;
    size_t child_count = 0U;

    memset(&model_plan, 0, sizeof(model_plan));
    memset(&logical_device, 0, sizeof(logical_device));
    memset(&data_set, 0, sizeof(data_set));
    memset(&report, 0, sizeof(report));
    snprintf(logical_device.inst, sizeof(logical_device.inst), "%s", "KINTE06SMPMEAS");
    snprintf(data_set.logical_device_inst, sizeof(data_set.logical_device_inst), "%s", "KINTE06SMPMEAS");
    snprintf(data_set.logical_node_name, sizeof(data_set.logical_node_name), "%s", "LLN0");
    snprintf(data_set.name, sizeof(data_set.name), "%s", "MEAS_RCB1");
    snprintf(report.logical_device_inst, sizeof(report.logical_device_inst), "%s", "KINTE06SMPMEAS");
    snprintf(report.logical_node_name, sizeof(report.logical_node_name), "%s", "LLN0");
    snprintf(report.name, sizeof(report.name), "%s", "urcbA");
    snprintf(report.rpt_id, sizeof(report.rpt_id), "%s", "KINTE06SMPMEAS/LLN0.urcbA");
    report.is_buffered = 0;
    report.data_set_index = 0U;
    report.conf_rev_known = 1;
    report.conf_rev = 1U;
    report.buffer_time_ms_known = 1;
    report.buffer_time_ms = 100U;
    report.integrity_period_ms_known = 1;
    report.integrity_period_ms = 1000U;
    report.optional_fields_mask = UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM
        | UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP
        | UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION
        | UNITLAB_IED_MODEL_RPT_OPT_DATA_SET
        | UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID
        | UNITLAB_IED_MODEL_RPT_OPT_CONF_REV;

    model_plan.logical_devices = &logical_device;
    model_plan.logical_device_count = 1U;
    model_plan.data_sets = &data_set;
    model_plan.data_set_count = 1U;
    model_plan.reports = &report;
    model_plan.report_count = 1U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &model_plan) == 1);
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(server_runtime_select_report_control_by_reference(&server_runtime, "KINTE06SMPMEAS.LLN0.RP.urcbA") == 1);
    assert(server_runtime_encode_report_control_block_value(
        &server_runtime,
        "KINTE06SMPMEAS/LLN0.urcbA",
        "KINTE06SMPMEAS/LLN0$MEAS_RCB1",
        rcb_bytes,
        sizeof(rcb_bytes),
        &rcb_length,
        &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);

    unitlab_mms_ber_element_init(&rcb_element);
    assert(unitlab_mms_ber_read(&rcb_element, rcb_bytes, rcb_length, &consumed_length, &diagnostic) == 1);
    assert(consumed_length == rcb_length);
    assert_ber_tag(&rcb_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 2U);
    while (child_offset < rcb_element.value_length) {
        size_t child_consumed_length = 0U;

        unitlab_mms_ber_element_init(&child_element);
        assert(unitlab_mms_ber_read(&child_element, &rcb_element.value_bytes[child_offset], rcb_element.value_length - child_offset, &child_consumed_length, &diagnostic) == 1);
        assert(child_count < sizeof(expected_tags) / sizeof(expected_tags[0]));
        assert_ber_tag(&child_element, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, expected_tags[child_count]);
        child_offset += child_consumed_length;
        child_count++;
    }
    assert(child_count == sizeof(expected_tags) / sizeof(expected_tags[0]));
    assert(contains_bytes(rcb_element.value_bytes, rcb_element.value_length, (const uint8_t*)"KINTE06SMPMEAS/LLN0.urcbA", strlen("KINTE06SMPMEAS/LLN0.urcbA")) == 1);
    assert(contains_bytes(rcb_element.value_bytes, rcb_element.value_length, (const uint8_t*)"KINTE06SMPMEAS/LLN0$MEAS_RCB1", strlen("KINTE06SMPMEAS/LLN0$MEAS_RCB1")) == 1);
    assert(contains_bytes(rcb_element.value_bytes, rcb_element.value_length, (const uint8_t*)"\x84\x03\x06\x79\x80", 5U) == 1);
}

int main(void)
{
    test_server_runtime_init_captures_default_snapshot();
    test_server_runtime_apply_model_plan_sets_active_model();
    test_server_runtime_prepare_start_stop();
    test_server_runtime_apply_association_request_bytes_accepts_acse_aarq();
    test_server_runtime_build_association_response_matches_reference_capture();
    test_server_runtime_apply_association_then_confirmed_request_keeps_session_associated();
    test_server_runtime_apply_association_request_bytes_accepts_captured_iedscout_aarq();
    test_server_runtime_apply_association_request_bytes_rejects_non_initiate_request();
    test_wire_builder_builds_confirmed_response_frame_roundtrips();
    test_server_runtime_build_confirmed_response_bytes_roundtrips();
    test_server_runtime_confirmed_response_fails_after_timeout();
    test_server_runtime_apply_reference_confirmed_request_and_build_response_roundtrips();
    test_server_runtime_apply_direct_read_request_and_build_response_roundtrips();
    test_server_runtime_apply_iedscout_namespace_multi_read_builds_response();
    test_server_runtime_apply_iedscout_buffered_report_control_block_read_builds_response();
    test_server_runtime_apply_iedscout_buffered_report_control_block_container_read_builds_response();
    test_server_runtime_build_brcb_read_uses_default_advertised_domain();
    test_server_runtime_model_urcb_read_uses_unbuffered_rcb_shape();
    test_server_runtime_build_brcb_scalar_multi_read_uses_direct_data_access_results();
    test_server_runtime_brcb_read_uses_model_report_dataset_reference();
    test_server_runtime_reads_model_backed_rcb_dataset_aliases();
    test_server_runtime_reads_model_backed_urcb_class_container();
    test_server_runtime_second_model_rcb_gi_uses_second_dataset();
    test_server_runtime_build_read_failure_uses_data_access_error_access_result();
    test_server_runtime_model_placeholder_reads_keep_discovery_flow_alive();
    test_server_runtime_mixed_multi_read_preserves_access_result_failures();
    test_server_runtime_build_ordinary_ln_gva_response_exposes_fc_roots();
    test_server_runtime_build_model_fc_root_gva_response_exposes_dataset_do_tree();
    test_server_runtime_build_model_report_gva_response_uses_report_control_names();
    test_server_runtime_build_model_lln0_gva_uses_report_class_presence();
    test_server_runtime_build_fc_root_reads_match_lib_shape();
    test_server_runtime_build_brcb_gva_response_exposes_fields();
    test_server_runtime_apply_release_request_builds_lib_shape_response();
    test_server_runtime_apply_get_name_list_request_and_build_response_roundtrips();
    test_server_runtime_build_get_name_list_response_handles_large_directory();
    test_server_runtime_apply_iedscout_get_name_list_request_matches_golden_capture();
    test_server_runtime_fixture_domain_get_name_list_uses_mms_domain();
    test_server_runtime_apply_iedscout_logical_node_directory_request_class_one_builds_response();
    test_server_runtime_apply_iedscout_vmd_directory_request_scope_zero_builds_response();
    test_server_runtime_apply_named_variable_list_attributes_request_and_build_response_roundtrips();
    test_server_runtime_apply_named_variable_list_attributes_request_and_build_response_matches_live_fixture_style();
    test_server_runtime_named_variable_list_attributes_returns_expanded_leaf_members();
    test_server_runtime_named_variable_list_attributes_handles_large_model_dataset();
    test_server_runtime_apply_iedscout_aa_specific_directory_request_scope_two_builds_response();
    test_server_runtime_apply_iedscout_vmd_get_variable_access_attributes_request_builds_response();
    test_server_runtime_build_confirmed_error_bytes_roundtrips();
    test_server_runtime_unsupported_confirmed_service_builds_confirmed_error();
    test_server_runtime_apply_iedscout_logical_node_directory_request_builds_response();
    test_server_runtime_build_confirmed_response_bytes_matches_fixture_style_object_reference();
    test_server_runtime_apply_confirmed_request_and_build_response_roundtrips();
    test_server_runtime_rejects_mismatched_confirmed_response_invoke_id();
    test_server_runtime_apply_pcap_resvtms_write_builds_response();
    test_server_runtime_apply_pcap_resvtms_write_rejects_when_enabled();
    test_server_runtime_apply_pcap_multi_rptena_write_builds_response();
    test_server_runtime_apply_pcap_multi_write_preserves_partial_failures();
    test_server_runtime_apply_write_request_and_build_response_roundtrips();
    test_server_runtime_rptena_write_updates_brcb_read_state();
    test_server_runtime_resvtms_owner_tracks_reservation_lifecycle();
    test_server_runtime_resvtms_no_br_alias_read_write_roundtrips();
    test_server_runtime_gi_write_queues_information_report();
    test_server_runtime_gi_report_uses_model_dataset_members();
    test_server_runtime_gi_report_handles_large_model_dataset();
    test_server_runtime_updates_quality_timestamp_next_to_value_leaf();
    test_server_runtime_queued_report_uses_event_value_snapshot();
    test_server_runtime_different_report_kinds_are_queued_in_order();
    test_server_runtime_quality_update_queues_quality_change_report();
    test_server_runtime_quality_timestamp_update_aliases_share_stored_value();
    test_server_runtime_gi_report_uses_stored_quality_timestamp_members();
    test_server_runtime_data_change_report_uses_stored_quality_timestamp_members();
    test_server_runtime_update_signal_value_changes_read_value();
    test_server_runtime_signal_update_aliases_share_stored_dataset_value();
    test_server_runtime_update_dataset_member_queues_report_when_enabled();
    test_server_runtime_multiple_dataset_updates_share_one_data_change_report();
    test_server_runtime_same_value_update_uses_data_update_trigger();
    test_server_runtime_data_change_trigger_disabled_does_not_queue_report();
    test_server_runtime_update_non_report_dataset_signal_does_not_queue_report();
    test_server_runtime_dataset_write_queues_data_change_report();
    test_server_runtime_dataset_write_second_member_marks_second_inclusion_bit();
    test_server_runtime_report_option_fields_use_model_masks();
    test_server_runtime_report_option_scalar_reads_use_model_masks();
    test_server_runtime_information_report_omits_disabled_optional_fields();
    test_server_runtime_data_change_reports_advance_sequence_and_entry_time();
    test_server_runtime_information_report_buffer_overflow_false_by_default();
    test_server_runtime_information_report_buffer_overflow_reports_and_clears();
    test_server_runtime_pending_report_queue_overflow_sets_bufovfl();
    test_server_runtime_full_report_queue_coalesces_before_drop_newest();
    test_server_runtime_dataset_write_without_rptena_does_not_queue_report();
    test_server_runtime_integrity_poll_queues_full_dataset_report();
    test_server_runtime_integrity_poll_uses_active_report_control();
    test_server_runtime_information_report_requires_model_dataset();
    test_server_runtime_gi_requires_enabled_rptena();
    test_server_runtime_disabled_rcb_accepts_option_and_trigger_writes();
    test_server_runtime_enabled_rcb_rejects_static_config_write();
    test_server_runtime_enabled_rcb_rejects_static_field_matrix();
    test_server_runtime_enabled_rcb_rejects_identity_and_owner_writes();
    test_server_runtime_enabled_rcb_rejects_resvtms_release_write();
    test_server_runtime_rejects_invalid_rcb_boolean_writes();
    test_server_runtime_unsupported_signal_write_returns_failure_result();
    test_server_runtime_rptena_false_clears_pending_gi();
    test_server_runtime_purgebuf_write_resets_brcb_runtime_state();
    test_server_runtime_apply_wire_pdu_requires_running_state();
    test_server_runtime_apply_incoming_bytes_roundtrips_and_consumes_exact_frame();
    return 0;
}
