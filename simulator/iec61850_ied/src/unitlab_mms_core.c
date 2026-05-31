#include "unitlab_mms_core.h"

#include <string.h>

static int set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
{
    if (diagnostic == NULL) {
        return 0;
    }
    diagnostic->code = code;
    if (message == NULL) {
        diagnostic->message[0] = '\0';
        return 1;
    }
    strncpy(diagnostic->message, message, sizeof(diagnostic->message) - 1U);
    diagnostic->message[sizeof(diagnostic->message) - 1U] = '\0';
    return 1;
}

static void runtime_event_clear(UnitLabMmsRuntimeEvent* event)
{
    if (event == NULL) {
        return;
    }
    memset(event, 0, sizeof(*event));
}


static void runtime_event_log_clear(UnitLabMmsRuntimeEventLog* event_log)
{
    if (event_log == NULL) {
        return;
    }
    memset(event_log, 0, sizeof(*event_log));
}

static void runtime_event_log_append(UnitLabMmsRuntimeEventLog* event_log, const UnitLabMmsRuntimeEvent* event)
{
    if (event_log == NULL || event == NULL) {
        return;
    }
    if (event_log->count >= UNITLAB_MMS_RUNTIME_EVENT_LOG_CAPACITY) {
        memmove(&event_log->events[0], &event_log->events[1], sizeof(event_log->events[0]) * (UNITLAB_MMS_RUNTIME_EVENT_LOG_CAPACITY - 1U));
        event_log->count = UNITLAB_MMS_RUNTIME_EVENT_LOG_CAPACITY - 1U;
    }
    event_log->events[event_log->count] = *event;
    event_log->count++;
}

static void runtime_event_set(
    UnitLabMmsRuntimeEvent* event,
    UnitLabMmsRuntimeEventKind kind,
    uint32_t state_before,
    uint32_t state_after,
    uint32_t invoke_id,
    size_t request_length,
    size_t response_length,
    UnitLabMmsDiagnosticCode diagnostic_code,
    const char* diagnostic_message)
{
    if (event == NULL) {
        return;
    }
    event->kind = kind;
    event->state_before = state_before;
    event->state_after = state_after;
    event->invoke_id = invoke_id;
    event->request_length = request_length;
    event->response_length = response_length;
    event->diagnostic_code = diagnostic_code;
    if (diagnostic_message == NULL) {
        event->diagnostic_message[0] = '\0';
        return;
    }
    strncpy(event->diagnostic_message, diagnostic_message, sizeof(event->diagnostic_message) - 1U);
    event->diagnostic_message[sizeof(event->diagnostic_message) - 1U] = '\0';
}

static void runtime_event_set_and_append(
    UnitLabMmsRuntimeEvent* event,
    UnitLabMmsRuntimeEventLog* event_log,
    UnitLabMmsRuntimeEventKind kind,
    uint32_t state_before,
    uint32_t state_after,
    uint32_t invoke_id,
    size_t request_length,
    size_t response_length,
    UnitLabMmsDiagnosticCode diagnostic_code,
    const char* diagnostic_message)
{
    runtime_event_set(event, kind, state_before, state_after, invoke_id, request_length, response_length, diagnostic_code, diagnostic_message);
    runtime_event_log_append(event_log, event);
}

void unitlab_mms_diagnostic_clear(UnitLabMmsDiagnostic* diagnostic)
{
    if (diagnostic == NULL) {
        return;
    }
    diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_OK;
    diagnostic->message[0] = '\0';
}

void unitlab_mms_runtime_event_init(UnitLabMmsRuntimeEvent* event)
{
    runtime_event_clear(event);
}


void unitlab_mms_runtime_event_log_init(UnitLabMmsRuntimeEventLog* event_log)
{
    runtime_event_log_clear(event_log);
}

size_t unitlab_mms_runtime_event_log_count(const UnitLabMmsRuntimeEventLog* event_log)
{
    return event_log == NULL ? 0U : event_log->count;
}

const UnitLabMmsRuntimeEvent* unitlab_mms_runtime_event_log_at(const UnitLabMmsRuntimeEventLog* event_log, size_t index)
{
    if (event_log == NULL || index >= event_log->count) {
        return NULL;
    }
    return &event_log->events[index];
}


void unitlab_mms_operation_result_init(UnitLabMmsOperationResult* result)
{
    if (result == NULL) {
        return;
    }
    memset(result, 0, sizeof(*result));
}

void unitlab_mms_operation_result_from_trace(UnitLabMmsOperationResult* result, int ok, const UnitLabMmsDiagnostic* diagnostic, const UnitLabMmsRuntimeEventLog* trace, const UnitLabMmsRuntimeEvent* event)
{
    if (result == NULL) {
        return;
    }
    unitlab_mms_operation_result_init(result);
    result->ok = ok;
    if (diagnostic != NULL) {
        result->diagnostic = *diagnostic;
    }
    if (trace != NULL) {
        result->trace = *trace;
    }
    if (event != NULL) {
        result->event = *event;
    }
}

void unitlab_mms_operation_result_from_event(UnitLabMmsOperationResult* result, int ok, const UnitLabMmsDiagnostic* diagnostic, const UnitLabMmsRuntimeEvent* event)
{
    unitlab_mms_operation_result_from_trace(result, ok, diagnostic, NULL, event);
}


void unitlab_mms_runtime_snapshot_init(UnitLabMmsRuntimeSnapshot* snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
}

void unitlab_mms_runtime_snapshot_capture(UnitLabMmsRuntimeSnapshot* snapshot, const UnitLabMmsSession* session, const UnitLabIec61850ReportControl* report_control, const UnitLabMmsTransportExchange* transport, const UnitLabMmsOperationResult* last_result)
{
    if (snapshot == NULL) {
        return;
    }
    unitlab_mms_runtime_snapshot_init(snapshot);
    if (session != NULL) {
        snapshot->session = *session;
    }
    if (report_control != NULL) {
        snapshot->report_control = *report_control;
    }
    if (transport != NULL) {
        snapshot->transport = *transport;
    }
    if (last_result != NULL) {
        snapshot->last_result = *last_result;
    }
}

void unitlab_mms_session_init(UnitLabMmsSession* session)
{
    if (session == NULL) {
        return;
    }
    session->state = UNITLAB_MMS_SESSION_DISCONNECTED;
    session->next_invoke_id = 1U;
    session->active_invoke_id = 0U;
    runtime_event_clear(&session->last_event);
    runtime_event_log_clear(&session->event_log);
}

void unitlab_mms_session_reset(UnitLabMmsSession* session)
{
    unitlab_mms_session_init(session);
}

uint32_t unitlab_mms_session_next_invoke_id(UnitLabMmsSession* session)
{
    if (session == NULL) {
        return 0U;
    }
    uint32_t current = session->next_invoke_id;
    if (current == 0U) {
        current = 1U;
    }
    if (session->next_invoke_id == UINT32_MAX) {
        session->next_invoke_id = 1U;
    }
    else {
        session->next_invoke_id++;
        if (session->next_invoke_id == 0U) {
            session->next_invoke_id = 1U;
        }
    }
    return current;
}

int unitlab_mms_session_is_associated(const UnitLabMmsSession* session)
{
    return session != NULL && session->state == UNITLAB_MMS_SESSION_ASSOCIATED;
}

int unitlab_mms_session_begin_association(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic)
{
    if (session == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required for association start.");
        return 0;
    }
    if (session->state != UNITLAB_MMS_SESSION_DISCONNECTED) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association can only begin from disconnected state.");
        runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION, session->state, session->state, 0U, 0U, 0U, diagnostic == NULL ? UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR : diagnostic->code, diagnostic == NULL ? "session is required for association start." : diagnostic->message);
        return 0;
    }
    session->state = UNITLAB_MMS_SESSION_ASSOCIATING;
    session->active_invoke_id = unitlab_mms_session_next_invoke_id(session);
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION, UNITLAB_MMS_SESSION_DISCONNECTED, session->state, session->active_invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_session_complete_association(UnitLabMmsSession* session, uint32_t invoke_id, UnitLabMmsDiagnostic* diagnostic)
{
    if (session == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required for association completion.");
        return 0;
    }
    if (session->state != UNITLAB_MMS_SESSION_ASSOCIATING) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association can only complete from associating state.");
        runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION, session->state, session->state, invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, diagnostic == NULL ? "session is required for association completion." : diagnostic->message);
        return 0;
    }
    if (session->active_invoke_id != invoke_id) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "association invoke id does not match active request.");
        runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION, session->state, session->state, invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, diagnostic == NULL ? "association invoke id does not match active request." : diagnostic->message);
        return 0;
    }
    session->state = UNITLAB_MMS_SESSION_ASSOCIATED;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION, UNITLAB_MMS_SESSION_ASSOCIATING, session->state, invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_session_begin_release(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic)
{
    if (session == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required for release.");
        return 0;
    }
    if (session->state != UNITLAB_MMS_SESSION_ASSOCIATED) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_NOT_ASSOCIATED, "release requires an associated session.");
        runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_RELEASE, session->state, session->state, session->active_invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_NOT_ASSOCIATED, diagnostic == NULL ? "session is required for release." : diagnostic->message);
        return 0;
    }
    session->state = UNITLAB_MMS_SESSION_RELEASING;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_RELEASE, UNITLAB_MMS_SESSION_ASSOCIATED, session->state, session->active_invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_session_abort(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic)
{
    if (session == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required for abort.");
        return 0;
    }
    UnitLabMmsSessionState before = session->state;
    session->state = UNITLAB_MMS_SESSION_ABORTED;
    session->active_invoke_id = 0U;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_ABORT, before, session->state, 0U, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

void unitlab_iec61850_report_control_init(UnitLabIec61850ReportControl* report_control)
{
    if (report_control == NULL) {
        return;
    }
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_DISABLED;
    runtime_event_clear(&report_control->last_event);
    runtime_event_log_clear(&report_control->event_log);
}

void unitlab_iec61850_report_control_reset(UnitLabIec61850ReportControl* report_control)
{
    unitlab_iec61850_report_control_init(report_control);
}

static int report_control_fail(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, UnitLabMmsRuntimeEventKind kind, const char* message)
{
    if (report_control != NULL) {
        runtime_event_set_and_append(&report_control->last_event, &report_control->event_log, kind, report_control->state, report_control->state, 0U, 0U, 0U, code, message);
    }
    set_diagnostic(diagnostic, code, message);
    return 0;
}

int unitlab_iec61850_report_control_reserve(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE, "report control is required for reserve.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE, "reserve requires a disabled report control.");
    }
    UnitLabIec61850ReportControlState before = report_control->state;
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_RESERVED;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&report_control->last_event, &report_control->event_log, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE, before, report_control->state, 0U, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_iec61850_report_control_enable(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE, "report control is required for enable.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_RESERVED) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_NOT_ASSOCIATED, UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE, "enable requires a reserved report control.");
    }
    UnitLabIec61850ReportControlState before = report_control->state;
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_ENABLED;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&report_control->last_event, &report_control->event_log, UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE, before, report_control->state, 0U, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_iec61850_report_control_request_gi(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI, "report control is required for GI.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_ENABLED) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_NOT_ASSOCIATED, UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI, "GI requires an enabled report control.");
    }
    UnitLabIec61850ReportControlState before = report_control->state;
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&report_control->last_event, &report_control->event_log, UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI, before, report_control->state, 0U, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_iec61850_report_control_disable(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE, "report control is required for disable.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_ENABLED && report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING && report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_REPORTING) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_NOT_ASSOCIATED, UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE, "disable requires an enabled report control.");
    }
    UnitLabIec61850ReportControlState before = report_control->state;
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_DISABLED;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&report_control->last_event, &report_control->event_log, UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE, before, report_control->state, 0U, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_iec61850_report_control_release(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE, "report control is required for release.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE, "release requires a disabled report control.");
    }
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&report_control->last_event, &report_control->event_log, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE, report_control->state, report_control->state, 0U, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

void unitlab_mms_transport_exchange_init(UnitLabMmsTransportExchange* exchange)
{
    if (exchange == NULL) {
        return;
    }
    memset(exchange, 0, sizeof(*exchange));
    runtime_event_clear(&exchange->last_event);
    runtime_event_log_clear(&exchange->event_log);
}

int unitlab_mms_transport_exchange_bind_request(UnitLabMmsTransportExchange* exchange, const uint8_t* request_bytes, size_t request_length, uint32_t invoke_id, UnitLabMmsDiagnostic* diagnostic)
{
    if (exchange == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "transport exchange is required for request binding.");
        return 0;
    }
    if (request_bytes == NULL && request_length != 0U) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "request bytes are required when request length is non-zero.");
        runtime_event_set_and_append(&exchange->last_event, &exchange->event_log, UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST, 0U, 0U, invoke_id, request_length, 0U, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "request bytes are required when request length is non-zero.");
        return 0;
    }
    exchange->request_bytes = request_bytes;
    exchange->request_length = request_length;
    exchange->invoke_id = invoke_id;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&exchange->last_event, &exchange->event_log, UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST, 0U, 0U, invoke_id, request_length, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_transport_exchange_bind_response(UnitLabMmsTransportExchange* exchange, uint8_t* response_bytes, size_t response_capacity, UnitLabMmsDiagnostic* diagnostic)
{
    if (exchange == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "transport exchange is required for response binding.");
        return 0;
    }
    if (response_bytes == NULL && response_capacity != 0U) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "response bytes are required when response capacity is non-zero.");
        runtime_event_set_and_append(&exchange->last_event, &exchange->event_log, UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_RESPONSE, 0U, 0U, exchange->invoke_id, 0U, response_capacity, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "response bytes are required when response capacity is non-zero.");
        return 0;
    }
    exchange->response_bytes = response_bytes;
    exchange->response_capacity = response_capacity;
    exchange->response_length = 0U;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&exchange->last_event, &exchange->event_log, UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_RESPONSE, 0U, 0U, exchange->invoke_id, 0U, response_capacity, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_transport_exchange_set_response_length(UnitLabMmsTransportExchange* exchange, size_t response_length, UnitLabMmsDiagnostic* diagnostic)
{
    if (exchange == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "transport exchange is required for response length.");
        return 0;
    }
    if (exchange->response_bytes == NULL && response_length != 0U) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "response buffer is not bound.");
        runtime_event_set_and_append(&exchange->last_event, &exchange->event_log, UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH, 0U, 0U, exchange->invoke_id, exchange->request_length, response_length, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "response buffer is not bound.");
        return 0;
    }
    if (response_length > exchange->response_capacity) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "response length exceeds bound capacity.");
        runtime_event_set_and_append(&exchange->last_event, &exchange->event_log, UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH, 0U, 0U, exchange->invoke_id, exchange->request_length, response_length, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "response length exceeds bound capacity.");
        return 0;
    }
    exchange->response_length = response_length;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&exchange->last_event, &exchange->event_log, UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH, 0U, 0U, exchange->invoke_id, exchange->request_length, response_length, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}
