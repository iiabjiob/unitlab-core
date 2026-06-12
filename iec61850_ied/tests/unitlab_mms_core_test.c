#include "protocols/mms/unitlab_mms_core.h"
#include "model/model_plan.h"
#include "server/unitlab_mms_server_runtime_internal.h"

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

    assert(unitlab_iec61850_report_control_release(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 2U);

    assert(unitlab_iec61850_report_control_reserve(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 3U);

    assert(unitlab_iec61850_report_control_enable(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 4U);

    assert(unitlab_iec61850_report_control_request_gi(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 5U);

    assert(unitlab_iec61850_report_control_accept_report(&report_control, 77U, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED);
    assert(report_control.last_event.invoke_id == 77U);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 6U);

    assert(unitlab_iec61850_report_control_disable(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 7U);

    assert(unitlab_iec61850_report_control_release(&report_control, &diagnostic) == 1);
    assert(report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(report_control.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE);
    assert(unitlab_mms_runtime_event_log_count(&report_control.event_log) == 8U);
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

    semantic_result.ok = 1;
    semantic_result.outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
    semantic_result.pdu.kind = UNITLAB_MMS_DECODED_PDU_ASSOCIATE_REQUEST;

    assert(unitlab_mms_runtime_apply_semantic_result(&session, NULL, &semantic_result, &operation_result) == 1);
    assert(session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(operation_result.ok == 1);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(operation_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION);
    assert(unitlab_mms_runtime_event_log_count(&operation_result.trace) == 1U);

    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_operation_result_init(&operation_result);
    semantic_result.ok = 1;
    semantic_result.outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
    semantic_result.pdu.kind = UNITLAB_MMS_DECODED_PDU_ASSOCIATE_RESPONSE;

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

static int name_list_contains(char** names, size_t count, const char* expected)
{
    if (expected == NULL) {
        return 0;
    }
    for (size_t index = 0U; index < count; index++) {
        if (names[index] != NULL && strcmp(names[index], expected) == 0) {
            return 1;
        }
    }
    return 0;
}

static int byte_sequence_contains(const uint8_t* buffer, size_t buffer_length, const uint8_t* expected, size_t expected_length)
{
    if (buffer == NULL || expected == NULL || expected_length == 0U || buffer_length < expected_length) {
        return 0;
    }
    for (size_t index = 0U; index + expected_length <= buffer_length; index++) {
        if (memcmp(&buffer[index], expected, expected_length) == 0) {
            return 1;
        }
    }
    return 0;
}

static void test_get_name_list_browse_collection_filters_continue_after(void)
{
    UnitLabMmsPendingRequest request;
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[1U];
    UnitLabIedModelDataSet data_sets[2U];
    UnitLabMmsDiagnostic diagnostic;
    char** names = NULL;
    size_t count = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(data_sets, 0, sizeof(data_sets));

    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsUpdates");

    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 1U;
    plan.logical_nodes = logical_nodes;
    plan.data_set_count = 2U;
    plan.data_sets = data_sets;

    unitlab_mms_pending_request_init(&request);
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_GET_NAME_LIST, 61U, 7U, 1000U, 100U, &diagnostic) == 1);
    request.browse_object_class = 2U;
    request.browse_object_scope = 1U;
    snprintf(request.browse_domain_id, sizeof(request.browse_domain_id), "%s", "LD0");
    snprintf(request.browse_continue_after, sizeof(request.browse_continue_after), "%s", "LLN0$dsEvents");

    assert(unitlab_mms_pending_request_collect_get_name_list_names(&request, &plan, &names, &count, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(count == 1U);
    assert(strcmp(names[0], "LLN0$dsUpdates") == 0);

    unitlab_free_ied_model_name_list(names, count);
}

static void test_collects_flattened_domain_named_variables_for_real_model_discovery(void)
{
    UnitLabMmsPendingRequest request;
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[2U];
    UnitLabIedModelSignal signals[1U];
    UnitLabIedModelReportControl reports[1U];
    UnitLabMmsDiagnostic diagnostic;
    char** names = NULL;
    size_t count = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(signals, 0, sizeof(signals));
    memset(reports, 0, sizeof(reports));

    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "RFLO1");
    snprintf(signals[0].logical_device_inst, sizeof(signals[0].logical_device_inst), "%s", "LD0");
    snprintf(signals[0].logical_node_name, sizeof(signals[0].logical_node_name), "%s", "RFLO1");
    snprintf(signals[0].data_object_name, sizeof(signals[0].data_object_name), "%s", "FltDiskm");
    snprintf(signals[0].data_attribute_path, sizeof(signals[0].data_attribute_path), "%s", "mag.f");
    snprintf(signals[0].fc, sizeof(signals[0].fc), "%s", "MX");
    snprintf(signals[0].data_set_entry_variable, sizeof(signals[0].data_set_entry_variable), "%s", "LD0/RFLO1$MX$FltDiskm$mag$f");
    snprintf(reports[0].logical_device_inst, sizeof(reports[0].logical_device_inst), "%s", "LD0");
    snprintf(reports[0].logical_node_name, sizeof(reports[0].logical_node_name), "%s", "LLN0");
    snprintf(reports[0].name, sizeof(reports[0].name), "%s", "brcbA");
    reports[0].is_buffered = 1;

    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 2U;
    plan.logical_nodes = logical_nodes;
    plan.signal_count = 1U;
    plan.signals = signals;
    plan.report_count = 1U;
    plan.reports = reports;

    unitlab_mms_pending_request_init(&request);
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_GET_NAME_LIST, 7U, 1U, 1000U, 100U, &diagnostic) == 1);
    request.browse_object_class = 0U;
    request.browse_object_scope = 1U;
    snprintf(request.browse_domain_id, sizeof(request.browse_domain_id), "%s", "LD0");

    assert(unitlab_mms_pending_request_collect_get_name_list_names(&request, &plan, &names, &count, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(name_list_contains(names, count, "LLN0"));
    assert(name_list_contains(names, count, "RFLO1"));
    assert(name_list_contains(names, count, "RFLO1$MX"));
    assert(name_list_contains(names, count, "RFLO1$MX$FltDiskm"));
    assert(name_list_contains(names, count, "RFLO1$MX$FltDiskm$mag"));
    assert(name_list_contains(names, count, "RFLO1$MX$FltDiskm$mag$f"));
    assert(name_list_contains(names, count, "LLN0$BR"));
    assert(name_list_contains(names, count, "LLN0$BR$brcbA"));
    assert(name_list_contains(names, count, "LLN0$BR$brcbA$DatSet"));
    for (size_t index = 1U; index < count; index++) {
        assert(strcmp(names[index - 1U], names[index]) < 0);
    }

    unitlab_free_ied_model_name_list(names, count);
}

static void test_collects_logical_node_variables_for_directory_browse_class_one(void)
{
    UnitLabMmsPendingRequest request;
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[2U];
    UnitLabIedModelSignal signals[2U];
    UnitLabMmsDiagnostic diagnostic;
    char** names = NULL;
    size_t count = 0U;

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

    unitlab_mms_pending_request_init(&request);
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_GET_NAME_LIST, 7U, 1U, 1000U, 100U, &diagnostic) == 1);
    request.browse_object_class = 1U;
    request.browse_object_scope = 1U;
    snprintf(request.browse_domain_id, sizeof(request.browse_domain_id), "%s", "LD0");
    snprintf(request.browse_continue_after, sizeof(request.browse_continue_after), "%s", "LLN0");

    assert(unitlab_mms_pending_request_collect_get_name_list_names(&request, &plan, &names, &count, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(count >= 7U);
    assert(strcmp(names[0], "Mod") == 0);
    assert(strcmp(names[1], "Beh") == 0);
    assert(strcmp(names[2], "Health") == 0);
    assert(strcmp(names[3], "CF") == 0);
    assert(strcmp(names[4], "DC") == 0);
    assert(strcmp(names[5], "BR") == 0);
    assert(strcmp(names[6], "EX") == 0);

    unitlab_free_ied_model_name_list(names, count);
}


static void test_model_gva_logical_node_root_uses_fc_groups(void)
{
    UnitLabMmsServerRuntime runtime;
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[1U];
    UnitLabIedModelSignal signals[3U];
    UnitLabMmsDiagnostic diagnostic;
    uint8_t response[1024U];
    size_t response_length = 0U;
    const uint8_t component_st[] = { 0x80U, 0x02U, 'S', 'T' };
    const uint8_t component_spcso8[] = { 0x80U, 0x06U, 'S', 'P', 'C', 'S', 'O', '8' };
    const uint8_t component_stval[] = { 0x80U, 0x05U, 's', 't', 'V', 'a', 'l' };
    const uint8_t component_q[] = { 0x80U, 0x01U, 'q' };
    const uint8_t component_t[] = { 0x80U, 0x01U, 't' };

    memset(&runtime, 0, sizeof(runtime));
    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(signals, 0, sizeof(signals));

    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "AIDDIZGGIO1");
    for (size_t index = 0U; index < 3U; index++) {
        snprintf(signals[index].logical_device_inst, sizeof(signals[index].logical_device_inst), "%s", "LD0");
        snprintf(signals[index].logical_node_name, sizeof(signals[index].logical_node_name), "%s", "AIDDIZGGIO1");
        snprintf(signals[index].fc, sizeof(signals[index].fc), "%s", "ST");
        snprintf(signals[index].data_object_name, sizeof(signals[index].data_object_name), "%s", "SPCSO8");
    }
    snprintf(signals[0].data_attribute_path, sizeof(signals[0].data_attribute_path), "%s", "stVal");
    snprintf(signals[1].data_attribute_path, sizeof(signals[1].data_attribute_path), "%s", "q");
    snprintf(signals[2].data_attribute_path, sizeof(signals[2].data_attribute_path), "%s", "t");

    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 1U;
    plan.logical_nodes = logical_nodes;
    plan.signal_count = 3U;
    plan.signals = signals;
    runtime.model_plan = &plan;

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(server_runtime_build_get_variable_access_attributes_response_service(&runtime, 12U, "LD0.AIDDIZGGIO1", response, sizeof(response), &response_length, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(byte_sequence_contains(response, response_length, component_st, sizeof(component_st)) == 1);
    assert(byte_sequence_contains(response, response_length, component_spcso8, sizeof(component_spcso8)) == 1);
    assert(byte_sequence_contains(response, response_length, component_stval, sizeof(component_stval)) == 1);
    assert(byte_sequence_contains(response, response_length, component_q, sizeof(component_q)) == 1);
    assert(byte_sequence_contains(response, response_length, component_t, sizeof(component_t)) == 1);
}

static void test_collects_vmd_named_variable_lists_for_browse_class_two_scope_zero(void)
{
    UnitLabMmsPendingRequest request;
    UnitLabIedModelPlan plan;
    UnitLabIedModelDataSet data_sets[4U];
    UnitLabMmsDiagnostic diagnostic;
    char** names = NULL;
    size_t count = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(data_sets, 0, sizeof(data_sets));

    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsEvents");
    snprintf(data_sets[2].name, sizeof(data_sets[2].name), "%s", "dsEvents");
    snprintf(data_sets[3].name, sizeof(data_sets[3].name), "%s", "dsWire");

    plan.data_set_count = 4U;
    plan.data_sets = data_sets;

    unitlab_mms_pending_request_init(&request);
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_GET_NAME_LIST, 62U, 8U, 1000U, 100U, &diagnostic) == 1);
    request.browse_object_class = 2U;
    request.browse_object_scope = 0U;

    assert(unitlab_mms_pending_request_collect_get_name_list_names(&request, &plan, &names, &count, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(count == 0U);
    assert(names == NULL);

    unitlab_free_ied_model_name_list(names, count);
}

static void test_collects_aa_specific_get_name_list_as_empty_list(void)
{
    UnitLabMmsPendingRequest request;
    UnitLabIedModelPlan plan;
    UnitLabIedModelDataSet data_sets[2U];
    UnitLabMmsDiagnostic diagnostic;
    char** names = NULL;
    size_t count = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(data_sets, 0, sizeof(data_sets));

    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsWire");

    plan.data_set_count = 2U;
    plan.data_sets = data_sets;

    unitlab_mms_pending_request_init(&request);
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_GET_NAME_LIST, 63U, 8U, 1000U, 100U, &diagnostic) == 1);
    request.browse_object_class = 2U;
    request.browse_object_scope = 2U;

    assert(unitlab_mms_pending_request_collect_get_name_list_names(&request, &plan, &names, &count, &diagnostic) == 1);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(count == 0U);
    assert(names == NULL);
}

static void test_runtime_apply_semantic_decode_failure(void)
{
    UnitLabMmsSession session;
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_session_init(&session);
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_operation_result_init(&operation_result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    semantic_result.ok = 0;
    semantic_result.outcome = UNITLAB_MMS_SERVICE_OUTCOME_ERROR;
    semantic_result.diagnostic.classification = UNITLAB_MMS_DECODE_CLASSIFICATION_DECODE_FAILURE;
    semantic_result.diagnostic.diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
    semantic_result.diagnostic.diagnostic.message[0] = '\0';

    assert(unitlab_mms_runtime_apply_semantic_result(&session, NULL, &semantic_result, &operation_result) == 0);
    assert(operation_result.ok == 0);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(operation_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_NONE);
    assert(unitlab_mms_runtime_event_log_count(&operation_result.trace) == 0U);
    assert(session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
}

static void test_runtime_apply_semantic_reject(void)
{
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsOperationResult operation_result;

    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_operation_result_init(&operation_result);

    semantic_result.ok = 1;
    semantic_result.outcome = UNITLAB_MMS_SERVICE_OUTCOME_REJECT;
    semantic_result.pdu.kind = UNITLAB_MMS_DECODED_PDU_REJECT;
    semantic_result.pdu.reject.reject_for_invoke_id = 19U;
    semantic_result.pdu.reject.reject_class = 3U;
    semantic_result.pdu.reject.reject_code = 7U;
    semantic_result.pdu.reject.service_error_code = 11U;

    assert(unitlab_mms_runtime_apply_semantic_result(NULL, NULL, &semantic_result, &operation_result) == 0);
    assert(operation_result.ok == 0);
    assert(operation_result.reject.reject_for_invoke_id == 19U);
    assert(operation_result.reject.reject_class == 3U);
    assert(operation_result.reject.reject_code == 7U);
    assert(operation_result.reject.service_error_code == 11U);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
}


static void test_runtime_apply_semantic_correlation_mismatch(void)
{
    UnitLabMmsPendingRequest request;
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;

    unitlab_mms_pending_request_init(&request);
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_operation_result_init(&operation_result);
    unitlab_mms_diagnostic_clear(&diagnostic);

    assert(unitlab_mms_pending_request_start(&request, UNITLAB_MMS_REQUEST_READ, 41U, 99U, 1000U, 10U, &diagnostic) == 1);
    semantic_result.ok = 1;
    semantic_result.outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
    semantic_result.pdu.kind = UNITLAB_MMS_DECODED_PDU_READ_RESPONSE;
    semantic_result.pdu.invoke_id = 77U;
    semantic_result.pdu.timestamp_ms = 15U;

    assert(unitlab_mms_runtime_apply_semantic_result(NULL, &request, &semantic_result, &operation_result) == 0);
    assert(operation_result.ok == 0);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVOKE_ID_MISMATCH);
    assert(operation_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_CORRELATION_MISMATCH);
    assert(operation_result.event.invoke_id == 77U);
    assert(operation_result.event.correlation_id == 99U);
    assert(request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(unitlab_mms_runtime_event_log_count(&request.event_log) == 1U);
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
    test_runtime_apply_semantic_reject();
    test_runtime_apply_semantic_correlation_mismatch();
    test_get_name_list_browse_collection_filters_continue_after();
    test_collects_vmd_named_variable_lists_for_browse_class_two_scope_zero();
    test_collects_flattened_domain_named_variables_for_real_model_discovery();
    test_collects_aa_specific_get_name_list_as_empty_list();
    test_collects_logical_node_variables_for_directory_browse_class_one();
    test_model_gva_logical_node_root_uses_fc_groups();
    test_runtime_apply_semantic_decode_failure();
    printf("unitlab-mms-core: ok\n");
    return 0;
}
