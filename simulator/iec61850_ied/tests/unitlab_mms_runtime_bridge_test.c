#include "unitlab_mms_core.h"
#include "unitlab_mms_runtime_bridge.h"

#include <assert.h>
#include <string.h>

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

int main(void)
{
    test_wire_read_response_applies_to_runtime();
    test_wire_information_report_applies_to_runtime();
    test_wire_correlation_mismatch_fails_closed();
    test_wire_reject_projection();
    return 0;
}
