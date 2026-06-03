#include "protocols/mms/unitlab_mms_core.h"
#include "protocols/mms/unitlab_mms_runtime_bridge.h"

#include <assert.h>
#include <string.h>

static void test_wire_associate_request_and_response_bridge(void)
{
    UnitLabMmsSession session;
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_session_init(&session);
    unitlab_mms_operation_result_init(&result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_INITIATE_REQUEST;

    assert(unitlab_mms_runtime_apply_wire_pdu(&session, NULL, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION);

    unitlab_mms_operation_result_init(&result);
    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_INITIATE_RESPONSE;

    assert(unitlab_mms_runtime_apply_wire_pdu(&session, NULL, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(session.state == UNITLAB_MMS_SESSION_ASSOCIATED);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION);
}

static void test_wire_release_request_and_response_bridge(void)
{
    UnitLabMmsSession session;
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_session_init(&session);
    unitlab_mms_operation_result_init(&result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_session_begin_association(&session, &diagnostic) == 1);
    assert(unitlab_mms_session_complete_association(&session, 1U, &diagnostic) == 1);

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_CONCLUDE_REQUEST;

    assert(unitlab_mms_runtime_apply_wire_pdu(&session, NULL, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(session.state == UNITLAB_MMS_SESSION_RELEASING);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_RELEASE);

    unitlab_mms_operation_result_init(&result);
    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_CONCLUDE_RESPONSE;

    assert(unitlab_mms_runtime_apply_wire_pdu(&session, NULL, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_RELEASED);
}

static void test_wire_read_request_starts_pending_request(void)
{
    UnitLabMmsSession session;
    UnitLabMmsPendingRequest request;
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_session_init(&session);
    unitlab_mms_pending_request_init(&request);
    unitlab_mms_operation_result_init(&result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 52U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_READ;

    assert(unitlab_mms_runtime_apply_wire_pdu(&session, &request, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(request.invoke_id == 52U);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED);
}

static void test_wire_read_response_applies_to_runtime(void)
{
    UnitLabMmsSession session;
    UnitLabMmsPendingRequest request;
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_session_init(&session);
    unitlab_mms_pending_request_init(&request);
    unitlab_mms_operation_result_init(&result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_READ, 41U, 7U, 1000U, 100U, &diagnostic) == 1);
    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 41U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_READ;

    assert(unitlab_mms_runtime_apply_wire_pdu(&session, &request, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_COMPLETED);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_COMPLETED);
}

static void test_wire_information_report_applies_to_runtime(void)
{
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;

    unitlab_mms_operation_result_init(&result);
    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    wire_pdu.service_length = 3U;

    assert(unitlab_mms_runtime_apply_wire_pdu(NULL, NULL, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
}

static void test_wire_information_report_applies_to_report_control(void)
{
    UnitLabIec61850ReportControl report_control;
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_iec61850_report_control_init(&report_control);
    unitlab_mms_operation_result_init(&result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_iec61850_report_control_reserve(&report_control, &diagnostic) == 1);
    assert(unitlab_iec61850_report_control_enable(&report_control, &diagnostic) == 1);
    assert(unitlab_iec61850_report_control_request_gi(&report_control, &diagnostic) == 1);

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    wire_pdu.invoke_id = 88U;
    wire_pdu.service_length = 3U;

    assert(unitlab_mms_runtime_apply_wire_pdu_with_report_control(NULL, NULL, &report_control, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED);
    assert(result.event.invoke_id == 88U);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED);
    assert(report_control.last_event.invoke_id == 88U);
}

static void test_wire_correlation_mismatch_fails_closed(void)
{
    UnitLabMmsSession session;
    UnitLabMmsPendingRequest request;
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_session_init(&session);
    unitlab_mms_pending_request_init(&request);
    unitlab_mms_operation_result_init(&result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_READ, 41U, 99U, 1000U, 100U, &diagnostic) == 1);
    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 77U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_READ;

    assert(unitlab_mms_runtime_apply_wire_pdu(&session, &request, &wire_pdu, &result) == 0);
    assert(result.ok == 0);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVOKE_ID_MISMATCH);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_CORRELATION_MISMATCH);
    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
}

static void test_wire_reject_projection(void)
{
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;

    unitlab_mms_operation_result_init(&result);
    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_REJECT;

    assert(unitlab_mms_runtime_apply_wire_pdu(NULL, NULL, &wire_pdu, &result) == 0);
    assert(result.ok == 0);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR || result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
}

static void test_wire_get_name_list_request_starts_pending_request(void)
{
    UnitLabMmsSession session;
    UnitLabMmsPendingRequest request;
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;
    const uint8_t payload[] = {
        0x30U, 0x0CU,
        0xA0U, 0x03U, 0x02U, 0x01U, 0x02U,
        0xA1U, 0x05U, 0x81U, 0x03U, 'L', 'D', '0'
    };

    unitlab_mms_session_init(&session);
    unitlab_mms_pending_request_init(&request);
    unitlab_mms_operation_result_init(&result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 61U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAME_LIST;
    wire_pdu.service_bytes = payload;
    wire_pdu.service_length = sizeof(payload);

    assert(unitlab_mms_runtime_apply_wire_pdu(&session, &request, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(request.invoke_id == 61U);
    assert(request.browse_object_class == 2U);
    assert(request.browse_object_scope == 1U);
    assert(strcmp(request.browse_domain_id, "LD0") == 0);
}

static void test_wire_get_name_list_response_applies_to_runtime(void)
{
    UnitLabMmsSession session;
    UnitLabMmsPendingRequest request;
    UnitLabMmsOperationResult result;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_session_init(&session);
    unitlab_mms_pending_request_init(&request);
    unitlab_mms_operation_result_init(&result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_GET_NAME_LIST, 61U, 7U, 1000U, 100U, &diagnostic) == 1);
    memset(&wire_pdu, 0, sizeof(wire_pdu));
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 61U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_GET_NAME_LIST;

    assert(unitlab_mms_runtime_apply_wire_pdu(&session, &request, &wire_pdu, &result) == 1);
    assert(result.ok == 1);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_COMPLETED);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_COMPLETED);
}

int main(void)
{
    test_wire_associate_request_and_response_bridge();
    test_wire_release_request_and_response_bridge();
    test_wire_read_request_starts_pending_request();
    test_wire_read_response_applies_to_runtime();
    test_wire_information_report_applies_to_runtime();
    test_wire_information_report_applies_to_report_control();
    test_wire_correlation_mismatch_fails_closed();
    test_wire_get_name_list_request_starts_pending_request();
    test_wire_get_name_list_response_applies_to_runtime();
    test_wire_reject_projection();
    return 0;
}
