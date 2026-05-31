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

    memset(&diagnostic, 0xA5, sizeof(diagnostic));
    memset(&session, 0xA5, sizeof(session));
    memset(&report_control, 0xA5, sizeof(report_control));
    memset(&exchange, 0xA5, sizeof(exchange));

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_session_init(&session);
    unitlab_iec61850_report_control_init(&report_control);
    unitlab_mms_transport_exchange_init(&exchange);
    unitlab_mms_operation_result_init(&operation_result);

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

    report_control.state = UNITLAB_IEC61850_REPORT_CONTROL_REPORTING;
    assert(unitlab_iec61850_report_control_disable(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 4U);

    assert(unitlab_iec61850_report_control_release(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 5U);
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

int main(void)
{
    test_defaults();
    test_invoke_id_sequence();
    test_transport_exchange_bindings();
    test_operation_result_projection();
    test_session_transitions();
    test_report_control_lifecycle();
    test_report_control_reset();
    printf("unitlab-mms-core: ok\n");
    return 0;
}
