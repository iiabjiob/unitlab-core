#include "protocols/mms/unitlab_mms_semantic_pdu.h"
#include "protocols/mms/unitlab_mms_wire_semantic_bridge.h"

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
        0x30U, 0x14U,
        0x1AU, 0x12U,
        'X', 'C', 'B', 'R', '1', '$', 'S', 'T', '$', 'P', 'o', 's', '$', 's', 't', 'V', 'a', 'l'
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
    assert(strcmp(result.pdu.object_reference, "Pos.stVal") == 0);
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
    test_wire_pdu_bridge_information_report();
    test_wire_pdu_bridge_reject();
    test_wire_pdu_bridge_get_name_list_request();
    test_wire_pdu_bridge_get_name_list_domain_request();
    test_wire_pdu_bridge_get_name_list_response();
    test_wire_pdu_bridge_rejects_unsupported_service();
    return 0;
}
