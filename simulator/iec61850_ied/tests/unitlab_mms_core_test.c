#include "unitlab_mms_core.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_defaults(void)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsSession session;
    UnitLabIec61850ReportControl report_control;
    UnitLabMmsTransportExchange exchange;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsRuntimeSnapshot snapshot;

    memset(&diagnostic, 0xA5, sizeof(diagnostic));
    memset(&session, 0xA5, sizeof(session));
    memset(&report_control, 0xA5, sizeof(report_control));
    memset(&exchange, 0xA5, sizeof(exchange));

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_init(&session);
    unitlab_iec61850_report_control_init(&report_control);
    unitlab_mms_transport_exchange_init(&exchange);
    unitlab_mms_operation_result_init(&operation_result);
    unitlab_mms_runtime_snapshot_init(&snapshot);

    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(diagnostic.message[0] == '\0');
    assert(session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
    assert(session.next_invoke_id == 1U);
    assert(session.active_invoke_id == 0U);
    assert(session.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_NONE);
    assert(unitlab_mms_runtime_event_log_count(&session.event_log) == 0U);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_NONE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 0U);
    assert(exchange.request_bytes == NULL);
    assert(exchange.request_length == 0U);
    assert(exchange.response_bytes == NULL);
    assert(exchange.response_capacity == 0U);
    assert(exchange.response_length == 0U);
    assert(exchange.invoke_id == 0U);
    assert(exchange.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_NONE);
    assert(unitlab_mms_runtime_event_log_count(&exchange.event_log) == 0U);
    assert(operation_result.ok == 0);
    assert(operation_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_NONE);
    assert(unitlab_mms_runtime_event_log_count(&operation_result.trace) == 0U);
    assert(snapshot.session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
    assert(snapshot.report_control_state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(snapshot.transport.invoke_id == 0U);
    assert(snapshot.last_result.ok == 0);
}

static void test_invoke_id_sequence(void)
{
    UnitLabMmsSession session;

    unitlab_mms_session_init(&session);
    assert(unitlab_mms_session_next_invoke_id(&session) == 1U);
    assert(unitlab_mms_session_next_invoke_id(&session) == 2U);
    session.next_invoke_id = UINT32_MAX;
    assert(unitlab_mms_session_next_invoke_id(&session) == UINT32_MAX);
    assert(session.next_invoke_id == 1U);
}

static void test_transport_exchange_bindings(void)
{
    UnitLabMmsTransportExchange exchange;
    UnitLabMmsDiagnostic diagnostic;
    uint8_t response_buffer[8];
    const uint8_t request_buffer[3] = { 0x01U, 0x02U, 0x03U };

    unitlab_mms_transport_exchange_init(&exchange);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_transport_exchange_bind_request(&exchange, request_buffer, sizeof(request_buffer), 17U, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(exchange.request_bytes == request_buffer);
    assert(exchange.request_length == sizeof(request_buffer));
    assert(exchange.invoke_id == 17U);
    assert(exchange.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST);
    assert(exchange.last_event.request_length == sizeof(request_buffer));
    assert(unitlab_mms_runtime_event_log_count(&exchange.event_log) == 1U);
    assert(unitlab_mms_runtime_event_log_at(&exchange.event_log, 0U) != NULL);

    assert(unitlab_mms_transport_exchange_bind_response(&exchange, response_buffer, sizeof(response_buffer), &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(exchange.response_bytes == response_buffer);
    assert(exchange.response_capacity == sizeof(response_buffer));
    assert(exchange.response_length == 0U);
    assert(exchange.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_RESPONSE);
    assert(exchange.last_event.response_length == sizeof(response_buffer));
    assert(unitlab_mms_runtime_event_log_count(&exchange.event_log) == 2U);

    assert(unitlab_mms_transport_exchange_set_response_length(&exchange, 4U, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(exchange.response_length == 4U);
    assert(exchange.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH);
    assert(exchange.last_event.response_length == 4U);
    assert(unitlab_mms_runtime_event_log_count(&exchange.event_log) == 3U);

    assert(unitlab_mms_transport_exchange_set_response_length(&exchange, sizeof(response_buffer) + 1U, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}


static void test_operation_result_projection(void)
{
    UnitLabMmsTransportExchange exchange;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult result;
    uint8_t response_buffer[4];
    const uint8_t request_buffer[2] = { 0xAAU, 0x55U };

    unitlab_mms_transport_exchange_init(&exchange);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_transport_exchange_bind_request(&exchange, request_buffer, sizeof(request_buffer), 9U, &diagnostic) == 1);
    unitlab_mms_operation_result_from_trace(&result, 1, &diagnostic, &exchange.event_log, &exchange.last_event);
    assert(result.ok == 1);
    assert(result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST);
    assert(unitlab_mms_runtime_event_log_count(&result.trace) == 1U);

    assert(unitlab_mms_transport_exchange_bind_response(&exchange, response_buffer, sizeof(response_buffer), &diagnostic) == 1);
    unitlab_mms_operation_result_from_trace(&result, 1, &diagnostic, &exchange.event_log, &exchange.last_event);
    assert(result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_RESPONSE);
    assert(unitlab_mms_runtime_event_log_count(&result.trace) == 2U);
}


static void test_runtime_snapshot_capture(void)
{
    UnitLabMmsSession session;
    UnitLabIec61850ReportControl report_control;
    UnitLabMmsTransportExchange transport;
    UnitLabMmsOperationResult result;
    UnitLabMmsRuntimeSnapshot snapshot;
    UnitLabMmsDiagnostic diagnostic;
    uint8_t response_buffer[2];
    const uint8_t request_buffer[1] = { 0x10U };

    unitlab_mms_session_init(&session);
    unitlab_iec61850_report_control_init(&report_control);
    unitlab_mms_transport_exchange_init(&transport);
    unitlab_mms_operation_result_init(&result);
    unitlab_mms_runtime_snapshot_init(&snapshot);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_session_begin_association(&session, &diagnostic) == 1);
    assert(unitlab_iec61850_report_control_reserve(&report_control, &diagnostic) == 1);
    assert(unitlab_mms_transport_exchange_bind_request(&transport, request_buffer, sizeof(request_buffer), 77U, &diagnostic) == 1);
    assert(unitlab_mms_transport_exchange_bind_response(&transport, response_buffer, sizeof(response_buffer), &diagnostic) == 1);
    unitlab_mms_operation_result_from_trace(&result, 1, &diagnostic, &transport.event_log, &transport.last_event);

    unitlab_mms_runtime_snapshot_capture(&snapshot, &session, &report_control, &transport, &result);
    assert(snapshot.session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(snapshot.report_control_state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED);
    assert(snapshot.transport.invoke_id == 77U);
    assert(snapshot.last_result.ok == 1);
    assert(snapshot.last_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_RESPONSE);
    assert(unitlab_mms_runtime_event_log_count(&snapshot.last_result.trace) == 2U);
    assert(snapshot.report_control_last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE);
    assert(unitlab_mms_runtime_event_log_count(&snapshot.report_control_event_log) == 1U);
}
static void test_session_transitions(void)
{
    UnitLabMmsSession session;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_session_init(&session);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_session_begin_association(&session, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(session.active_invoke_id == 1U);
    assert(session.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION);
    assert(unitlab_mms_runtime_event_log_count(&session.event_log) == 1U);

    assert(unitlab_mms_session_complete_association(&session, 999U, &diagnostic) == 0);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(session.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION);
    assert(unitlab_mms_runtime_event_log_count(&session.event_log) == 2U);

    assert(unitlab_mms_session_complete_association(&session, 1U, &diagnostic) == 1);
    assert(session.state == UNITLAB_MMS_SESSION_ASSOCIATED);
    assert(unitlab_mms_session_is_associated(&session) == 1);

    assert(unitlab_mms_session_begin_release(&session, &diagnostic) == 1);
    assert(session.state == UNITLAB_MMS_SESSION_RELEASING);
    assert(session.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_RELEASE);
    assert(unitlab_mms_runtime_event_log_count(&session.event_log) == 4U);

    assert(unitlab_mms_session_abort(&session, &diagnostic) == 1);
    assert(session.state == UNITLAB_MMS_SESSION_ABORTED);
    assert(session.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_ABORT);
    assert(unitlab_mms_runtime_event_log_count(&session.event_log) == 5U);
}

static void test_report_control_lifecycle(void)
{
    UnitLabIec61850ReportControl report_control;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_iec61850_report_control_init(&report_control);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_iec61850_report_control_reserve(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 1U);

    assert(unitlab_iec61850_report_control_enable(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 2U);

    assert(unitlab_iec61850_report_control_request_gi(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 3U);

    assert(unitlab_iec61850_report_control_accept_report(&report_control, 77U, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED);
    assert(report_control.last_event.invoke_id == 77U);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 4U);

    assert(unitlab_iec61850_report_control_disable(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 5U);

    assert(unitlab_iec61850_report_control_release(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 6U);
}

static void test_report_control_reset(void)
{
    UnitLabIec61850ReportControl report_control;

    report_control.state = UNITLAB_IEC61850_REPORT_CONTROL_REPORTING;
    unitlab_iec61850_report_control_reset(&report_control);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_NONE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 0U);
}

static void test_typed_event_and_aliases(void)
{
    assert(UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION == 1);
    assert(UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION == 2);
    assert(UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_RELEASE == 3);
    assert(UNITLAB_MMS_RUNTIME_EVENT_SESSION_ABORT == 4);
    assert(UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE == UNITLAB_MMS_RUNTIME_EVENT_RCB_RESERVED);
    assert(UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE == UNITLAB_MMS_RUNTIME_EVENT_RCB_ENABLED);
    assert(UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI == UNITLAB_MMS_RUNTIME_EVENT_GI_REQUESTED);
    assert(UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE == UNITLAB_MMS_RUNTIME_EVENT_RCB_DISABLED);
    assert(UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE == UNITLAB_MMS_RUNTIME_EVENT_RCB_RELEASED);
    assert(UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_BOUND);
    assert(UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_RESPONSE == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_COMPLETED);
}

static void test_pending_request_lifecycle(void)
{
    UnitLabMmsPendingRequest request;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_pending_request_init(&request);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_IDLE);
    assert(request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_NONE);

    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_READ, 41U, 7U, 1000U, 100U, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(request.kind == UNITLAB_MMS_REQUEST_READ);
    assert(request.invoke_id == 41U);
    assert(request.correlation_id == 7U);
    assert(request.deadline_ms == 1000U);
    assert(request.timestamp_ms == 100U);
    assert(request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED);
    assert(request.last_event.request_kind == UNITLAB_MMS_REQUEST_READ);
    assert(request.last_event.correlation_id == 7U);
    assert(request.last_event.deadline_ms == 1000U);
    assert(unitlab_mms_runtime_event_log_count(&request.event_log) == 1U);

    assert(unitlab_mms_pending_request_complete(&request, 150U, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_COMPLETED);
    assert(request.completed == 1);
    assert(request.timed_out == 0);
    assert(request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_COMPLETED);
    assert(request.last_event.timestamp_ms == 150U);
    assert(request.last_event.completed == 1);
    assert(unitlab_mms_runtime_event_log_count(&request.event_log) == 2U);

    unitlab_mms_pending_request_init(&request);
    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_WRITE, 42U, 8U, 2000U, 200U, &diagnostic) == 1);
    assert(unitlab_mms_pending_request_mark_timed_out(&request, 2200U, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_TIMEOUT);
    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_TIMED_OUT);
    assert(request.timed_out == 1);
    assert(request.completed == 0);
    assert(request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_TIMED_OUT);
    assert(request.last_event.deadline_ms == 2000U);
    assert(request.last_event.timestamp_ms == 2200U);
    assert(request.last_event.timed_out == 1);
    assert(unitlab_mms_runtime_event_log_count(&request.event_log) == 2U);
}

static void test_semantic_pdu_defaults(void)
{
    UnitLabMmsAssociateRequest associate_request;
    UnitLabMmsReadRequest read_request;
    UnitLabMmsWriteRequest write_request;
    UnitLabMmsInformationReport report;

    unitlab_mms_associate_request_init(&associate_request);
    unitlab_mms_read_request_init(&read_request);
    unitlab_mms_write_request_init(&write_request);
    unitlab_mms_information_report_init(&report);

    assert(associate_request.invoke_id == 0U);
    assert(associate_request.deadline_ms == 0U);
    assert(associate_request.calling_ae_title[0] == '\0');
    assert(read_request.object_reference[0] == '\0');
    assert(read_request.attribute_reference[0] == '\0');
    assert(write_request.value_bytes == NULL);
    assert(write_request.value_length == 0U);
    assert(report.report_control_reference[0] == '\0');
    assert(report.data_set_reference[0] == '\0');
    assert(report.item_count == 0U);
    assert(report.buffered == 0);
}

static void test_runtime_apply_semantic_result(void)
{
    UnitLabMmsSession session;
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_session_init(&session);
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_operation_result_init(&operation_result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_session_begin_association(&session, &diagnostic) == 1);
    semantic_result.ok = 1;
    semantic_result.outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
    semantic_result.pdu.kind = UNITLAB_MMS_DECODED_PDU_ASSOCIATE_RESPONSE;
    semantic_result.pdu.invoke_id = session.active_invoke_id;

    assert(unitlab_mms_runtime_apply_semantic_result(&session, NULL, &semantic_result, &operation_result) == 1);
    assert(session.state == UNITLAB_MMS_SESSION_ASSOCIATED);
    assert(operation_result.ok == 1);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(operation_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION);
    assert(unitlab_mms_runtime_event_log_count(&operation_result.trace) == 2U);

    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_operation_result_init(&operation_result);
    semantic_result.ok = 1;
    semantic_result.outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
    semantic_result.pdu.kind = UNITLAB_MMS_DECODED_PDU_INFORMATION_REPORT;
    semantic_result.pdu.invoke_id = 55U;
    semantic_result.pdu.correlation_id = 12U;
    semantic_result.pdu.timestamp_ms = 1234U;

    assert(unitlab_mms_runtime_apply_semantic_result(NULL, NULL, &semantic_result, &operation_result) == 1);
    assert(operation_result.ok == 1);
    assert(operation_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED);
    assert(operation_result.event.invoke_id == 55U);
    assert(operation_result.event.correlation_id == 12U);
    assert(unitlab_mms_runtime_event_log_count(&operation_result.trace) == 1U);
}

int main(void)
{
    test_defaults();
    test_invoke_id_sequence();
    test_transport_exchange_bindings();
    test_operation_result_projection();
    test_runtime_snapshot_capture();
    test_session_transitions();
    test_report_control_lifecycle();
    test_report_control_reset();
    test_typed_event_and_aliases();
    test_pending_request_lifecycle();
    test_semantic_pdu_defaults();
    test_runtime_apply_semantic_result();
    printf("unitlab-mms-core: ok\n");
    return 0;
}
