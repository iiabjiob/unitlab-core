#include "protocols/mms/unitlab_mms_semantic_pdu.h"
#include "protocols/mms/unitlab_mms_wire_semantic_bridge.h"
#include "wire/mms/unitlab_mms_pdu.h"

#include <assert.h>
#include <string.h>

static void test_defaults(void)
{
    UnitLabMmsDecodedPdu pdu;
    UnitLabMmsDecodeDiagnostic diagnostic;
    UnitLabMmsSemanticResult result;

    memset(&pdu, 0xA5, sizeof(pdu));
    memset(&diagnostic, 0xA5, sizeof(diagnostic));
    memset(&result, 0xA5, sizeof(result));

    unitlab_mms_decoded_pdu_init(&pdu);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    unitlab_mms_semantic_result_init(&result);

    assert(pdu.kind == UNITLAB_MMS_DECODED_PDU_NONE);
    assert(pdu.invoke_id == 0U);
    assert(pdu.value_bytes == NULL);
    assert(pdu.value_length == 0U);
    assert(pdu.reject.reject_for_invoke_id == 0U);
    assert(diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
    assert(diagnostic.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(diagnostic.detail[0] == '\0');
    assert(result.ok == 0);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_NONE);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_NONE);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_diagnostic_set(void)
{
    UnitLabMmsDecodeDiagnostic diagnostic;

    unitlab_mms_decode_diagnostic_init(&diagnostic);
    unitlab_mms_decode_diagnostic_set(&diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_DECODE_FAILURE, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "wire parse error", "malformed MMS packet");

    assert(diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_DECODE_FAILURE);
    assert(diagnostic.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(strcmp(diagnostic.detail, "wire parse error") == 0);
    assert(strcmp(diagnostic.diagnostic.message, "malformed MMS packet") == 0);
}

static void test_result_projection(void)
{
    UnitLabMmsDecodedPdu pdu;
    UnitLabMmsDecodeDiagnostic diagnostic;
    UnitLabMmsSemanticResult result;

    unitlab_mms_decoded_pdu_init(&pdu);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    unitlab_mms_semantic_result_init(&result);

    pdu.kind = UNITLAB_MMS_DECODED_PDU_INFORMATION_REPORT;
    pdu.value_bytes = (const uint8_t*)"abc";
    pdu.value_length = 3U;
    unitlab_mms_semantic_result_from_decoded_pdu(&result, UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS, &pdu, &diagnostic);

    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_INFORMATION_REPORT);
    assert(result.pdu.value_length == 3U);

    unitlab_mms_decode_diagnostic_set(&diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_DECODE_FAILURE, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "bad frame", "bad frame");
    unitlab_mms_semantic_result_from_decoded_pdu(&result, UNITLAB_MMS_SERVICE_OUTCOME_ERROR, &pdu, &diagnostic);

    assert(result.ok == 0);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_NONE);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_NONE);
}

static void test_reject_projection(void)
{
    UnitLabMmsDecodedPdu pdu;
    UnitLabMmsDecodeDiagnostic diagnostic;
    UnitLabMmsSemanticResult result;

    unitlab_mms_decoded_pdu_init(&pdu);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    unitlab_mms_semantic_result_init(&result);

    pdu.kind = UNITLAB_MMS_DECODED_PDU_REJECT;
    pdu.reject.reject_for_invoke_id = 19U;
    pdu.reject.reject_class = 3U;
    pdu.reject.reject_code = 7U;
    pdu.reject.service_error_code = 11U;
    unitlab_mms_semantic_result_from_decoded_pdu(&result, UNITLAB_MMS_SERVICE_OUTCOME_REJECT, &pdu, &diagnostic);

    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_REJECT);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_REJECT);
    assert(result.pdu.reject.reject_for_invoke_id == 19U);
    assert(result.pdu.reject.reject_class == 3U);
    assert(result.pdu.reject.reject_code == 7U);
    assert(result.pdu.reject.service_error_code == 11U);
}

static void test_wire_pdu_bridge_read_request(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;
    const uint8_t payload[] = {
        0x30U, 0x1FU,
        0xA1U, 0x1DU,
        0xA0U, 0x1BU,
        0x30U, 0x19U,
        0xA0U, 0x17U,
        0xA1U, 0x15U,
        0x1AU, 0x05U, 'X', 'C', 'B', 'R', '1',
        0x1AU, 0x0CU, 'S', 'T', '$', 'P', 'o', 's', '$', 's', 't', 'V', 'a', 'l'
    };

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 17U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_READ;
    wire_pdu.service_bytes = payload;
    wire_pdu.service_length = sizeof(payload);

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_READ_REQUEST);
    assert(result.pdu.invoke_id == 17U);
    assert(strcmp(result.pdu.domain_id, "XCBR1") == 0);
    assert(strcmp(result.pdu.item_id, "ST$Pos$stVal") == 0);
    assert(strcmp(result.pdu.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(strcmp(result.pdu.attribute_reference, "stVal") == 0);
    assert(result.pdu.value_length == sizeof(payload));
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_information_report(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;
    const uint8_t payload[3] = { 0x81U, 0x01U, 0x00U };

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    wire_pdu.service_bytes = payload;
    wire_pdu.service_length = sizeof(payload);

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_INFORMATION_REPORT);
    assert(result.pdu.value_length == sizeof(payload));
    assert(result.pdu.invoke_id == 0U);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_initiate_request(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_INITIATE_REQUEST;
    wire_pdu.invoke_id = 1U;

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_ASSOCIATE_REQUEST);
    assert(result.pdu.invoke_id == 1U);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_write_request(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;
    const uint8_t payload[] = {
        0x02U, 0x01U, 0x2BU,
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

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 43U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_WRITE;
    wire_pdu.service_bytes = &payload[3 + 2];
    wire_pdu.service_length = sizeof(payload) - (3U + 2U);

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_WRITE_REQUEST);
    assert(result.pdu.invoke_id == 43U);
    assert(strcmp(result.pdu.domain_id, "XCBR1") == 0);
    assert(strcmp(result.pdu.item_id, "ST$Pos$stVal") == 0);
    assert(strcmp(result.pdu.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(strcmp(result.pdu.attribute_reference, "stVal") == 0);
    assert(result.pdu.value_length == 1U);
    assert(result.pdu.value_bytes[0] == 0xFFU);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_initiate_response(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_INITIATE_RESPONSE;
    wire_pdu.invoke_id = 1U;

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_ASSOCIATE_RESPONSE);
    assert(result.pdu.invoke_id == 1U);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_reject(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_REJECT;

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_REJECT);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_REJECT);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_conclude_error(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONCLUDE_ERROR;

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_ERROR);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_ABORT);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_rejects_unsupported_service(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 7U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_RAW;

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 0);
    assert(result.ok == 0);
    assert(diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC);
}

static void test_wire_pdu_bridge_get_variable_access_attributes_request(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;
    const uint8_t payload[] = {
        0xA0U, 0x17U,
        0xA1U, 0x15U,
        0x1AU, 0x05U, 'X', 'C', 'B', 'R', '1',
        0x1AU, 0x0CU, 'S', 'T', '$', 'P', 'o', 's', '$', 's', 't', 'V', 'a', 'l'
    };

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 19U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES;
    wire_pdu.service_bytes = payload;
    wire_pdu.service_length = sizeof(payload);

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_REQUEST);
    assert(strcmp(result.pdu.domain_id, "XCBR1") == 0);
    assert(strcmp(result.pdu.item_id, "ST$Pos$stVal") == 0);
    assert(strcmp(result.pdu.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(strcmp(result.pdu.attribute_reference, "stVal") == 0);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_get_name_list_request(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;
    const uint8_t payload[] = {
        0x30U, 0x09U,
        0xA0U, 0x03U, 0x02U, 0x01U, 0x09U,
        0xA1U, 0x02U, 0x80U, 0x00U
    };

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 17U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAME_LIST;
    wire_pdu.service_bytes = payload;
    wire_pdu.service_length = sizeof(payload);

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_REQUEST);
    assert(result.pdu.object_class == 9U);
    assert(result.pdu.object_scope == 0U);
    assert(result.pdu.domain_id[0] == '\0');
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_get_name_list_domain_request(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;
    const uint8_t payload[] = {
        0x30U, 0x0CU,
        0xA0U, 0x03U, 0x02U, 0x01U, 0x02U,
        0xA1U, 0x05U, 0x81U, 0x03U, 'L', 'D', '0'
    };

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 18U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAME_LIST;
    wire_pdu.service_bytes = payload;
    wire_pdu.service_length = sizeof(payload);

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_REQUEST);
    assert(result.pdu.object_class == 2U);
    assert(result.pdu.object_scope == 1U);
    assert(strcmp(result.pdu.domain_id, "LD0") == 0);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_get_name_list_logical_node_directory_request(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;
    const uint8_t payload[] = {
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

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 3U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAME_LIST;
    wire_pdu.service_bytes = payload;
    wire_pdu.service_length = sizeof(payload);

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_REQUEST);
    assert(result.pdu.object_class == 3U);
    assert(result.pdu.object_scope == 1U);
    assert(strcmp(result.pdu.domain_id, "LD0") == 0);
    assert(strcmp(result.pdu.continue_after, "LLN0") == 0);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_wire_pdu_bridge_get_variable_access_attributes_response(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 19U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES;

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_RESPONSE);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}



static void test_wire_pdu_bridge_get_named_variable_list_attributes_requests(void)
{
    struct {
        const uint8_t* bytes;
        size_t length;
        uint32_t invoke_id;
        const char* domain_id;
        const char* item_id;
        const char* object_reference;
        const char* attribute_reference;
    } cases[] = {
        { (const uint8_t[]){ 0x02U, 0x01U, 0x0BU, 0xACU, 0x0AU, 0x80U, 0x08U, 'd', 's', 'E', 'v', 'e', 'n', 't', 's' }, 15U, 11U, "", "dsEvents", "dsEvents", "dsEvents" },
        { (const uint8_t[]){ 0x02U, 0x01U, 0x0CU, 0xACU, 0x08U, 0x80U, 0x06U, 'd', 's', 'W', 'i', 'r', 'e' }, 13U, 12U, "", "dsWire", "dsWire", "dsWire" },
        { (const uint8_t[]){ 0x02U, 0x01U, 0x0DU, 0xACU, 0x16U, 0xA1U, 0x14U, 0x1AU, 0x03U, 'L', 'D', '0', 0x1AU, 0x0DU, 'L', 'L', 'N', '0', '$', 'd', 's', 'E', 'v', 'e', 'n', 't', 's' }, 27U, 13U, "LD0", "LLN0$dsEvents", "LD0/LLN0$dsEvents", "LLN0$dsEvents" },
        { (const uint8_t[]){ 0x02U, 0x01U, 0x0EU, 0xACU, 0x15U, 0xA1U, 0x13U, 0x1AU, 0x03U, 'L', 'D', '0', 0x1AU, 0x0CU, 'G', 'G', 'I', 'O', '1', '$', 'd', 's', 'W', 'i', 'r', 'e' }, 26U, 14U, "LD0", "GGIO1$dsWire", "LD0/GGIO1$dsWire", "GGIO1$dsWire" },
    };

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++) {
        UnitLabMmsPdu request_pdu;
        UnitLabMmsPdu wire_pdu;
        UnitLabMmsSemanticResult result;
        UnitLabMmsDecodeDiagnostic diagnostic;
        UnitLabMmsDiagnostic pdu_diagnostic;
        uint8_t encoded_pdu[64U];
        size_t encoded_length = 0U;
        size_t consumed_length = 0U;

        unitlab_mms_pdu_init(&request_pdu);
        unitlab_mms_pdu_init(&wire_pdu);
        unitlab_mms_semantic_result_init(&result);
        unitlab_mms_decode_diagnostic_init(&diagnostic);
        unitlab_mms_diagnostic_clear(&pdu_diagnostic);

        request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
        request_pdu.pdu_bytes = cases[index].bytes;
        request_pdu.pdu_length = cases[index].length;
        assert(unitlab_mms_pdu_encode(&request_pdu, encoded_pdu, sizeof(encoded_pdu), &encoded_length, &pdu_diagnostic) == 1);
        assert(unitlab_mms_pdu_decode(&wire_pdu, encoded_pdu, encoded_length, &consumed_length, &pdu_diagnostic) == 1);
        assert(consumed_length == encoded_length);
        assert(wire_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
        assert(wire_pdu.has_invoke_id == 1);
        assert(wire_pdu.invoke_id == cases[index].invoke_id);
        assert(wire_pdu.has_service == 1);
        assert(wire_pdu.service_kind == UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES);
        assert(wire_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
        assert(wire_pdu.service_tag.constructed == 1);
        assert(wire_pdu.service_tag.tag_number == 12U);

        assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
        assert(result.ok == 1);
        assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
        assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_REQUEST);
        assert(result.pdu.invoke_id == cases[index].invoke_id);
        assert(strcmp(result.pdu.domain_id, cases[index].domain_id) == 0);
        assert(strcmp(result.pdu.item_id, cases[index].item_id) == 0);
        assert(strcmp(result.pdu.object_reference, cases[index].object_reference) == 0);
        assert(strcmp(result.pdu.attribute_reference, cases[index].attribute_reference) == 0);
        assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
    }
}

static void test_wire_pdu_bridge_get_name_list_response(void)
{
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsSemanticResult result;
    UnitLabMmsDecodeDiagnostic diagnostic;

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    unitlab_mms_semantic_result_init(&result);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 17U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAME_LIST;

    assert(unitlab_mms_semantic_result_from_wire_pdu(&result, &wire_pdu, &diagnostic) == 1);
    assert(result.ok == 1);
    assert(result.outcome == UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_RESPONSE);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

int main(void)
{
    test_defaults();
    test_diagnostic_set();
    test_result_projection();
    test_reject_projection();
    test_wire_pdu_bridge_read_request();
    test_wire_pdu_bridge_get_variable_access_attributes_request();
    test_wire_pdu_bridge_get_variable_access_attributes_response();
    test_wire_pdu_bridge_information_report();
    test_wire_pdu_bridge_initiate_request();
    test_wire_pdu_bridge_write_request();
    test_wire_pdu_bridge_initiate_response();
    test_wire_pdu_bridge_reject();
    test_wire_pdu_bridge_conclude_error();
    test_wire_pdu_bridge_get_name_list_request();
    test_wire_pdu_bridge_get_name_list_domain_request();
    test_wire_pdu_bridge_get_name_list_logical_node_directory_request();
    test_wire_pdu_bridge_get_named_variable_list_attributes_requests();
    test_wire_pdu_bridge_get_name_list_response();
    test_wire_pdu_bridge_rejects_unsupported_service();
    return 0;
}
