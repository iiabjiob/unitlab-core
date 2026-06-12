#include "protocols/mms/unitlab_mms_core.h"
#include "model/model_plan.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
    event->request_kind = 0U;
    event->state_before = state_before;
    event->state_after = state_after;
    event->invoke_id = invoke_id;
    event->correlation_id = 0U;
    event->timestamp_ms = 0U;
    event->deadline_ms = 0U;
    event->request_length = request_length;
    event->response_length = response_length;
    event->timed_out = 0;
    event->completed = 0;
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

static void operation_result_project_from_runtime(
    UnitLabMmsOperationResult* operation_result,
    int ok,
    const UnitLabMmsDiagnostic* diagnostic,
    const UnitLabMmsRuntimeEventLog* trace,
    const UnitLabMmsRuntimeEvent* event)
{
    UnitLabMmsDiagnostic diagnostic_copy;

    if (operation_result == NULL) {
        return;
    }
    if (diagnostic != NULL) {
        diagnostic_copy = *diagnostic;
        unitlab_mms_operation_result_from_trace(operation_result, ok, &diagnostic_copy, trace, event);
        return;
    }
    unitlab_mms_operation_result_from_trace(operation_result, ok, NULL, trace, event);
}

static void unitlab_mms_free_name_list_prefix(char** names, size_t count)
{
    if (names == NULL) {
        return;
    }
    for (size_t index = 0U; index < count; index++) {
        free(names[index]);
    }
}

static int unitlab_mms_filter_browse_continue_after(char*** names, size_t* count, const char* continue_after, UnitLabMmsDiagnostic* diagnostic)
{
    size_t start_index = 0U;
    int found = 0;

    if (names == NULL || count == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "browse name list and count are required.");
        return 0;
    }
    if (continue_after == NULL || continue_after[0] == '\0') {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (*names == NULL || *count == 0U) {
        if (*names != NULL) {
            free(*names);
            *names = NULL;
        }
        *count = 0U;
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "continueAfter cursor was not found in the browse result.");
        return 0;
    }

    for (size_t index = 0U; index < *count; index++) {
        if ((*names)[index] != NULL && strcmp((*names)[index], continue_after) == 0) {
            start_index = index + 1U;
            found = 1;
            break;
        }
    }

    if (!found) {
        unitlab_mms_free_name_list_prefix(*names, *count);
        free(*names);
        *names = NULL;
        *count = 0U;
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "continueAfter cursor was not found in the browse result.");
        return 0;
    }
    if (start_index == 0U) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (start_index >= *count) {
        unitlab_mms_free_name_list_prefix(*names, *count);
        free(*names);
        *names = NULL;
        *count = 0U;
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }

    unitlab_mms_free_name_list_prefix(*names, start_index);
    memmove(*names, *names + start_index, sizeof((*names)[0]) * (*count - start_index));
    *count -= start_index;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_pending_request_collect_get_name_list_names(const UnitLabMmsPendingRequest* request, const UnitLabIedModelPlan* plan, char*** names, size_t* count, UnitLabMmsDiagnostic* diagnostic)
{
    char model_error[256U];

    if (names != NULL) {
        *names = NULL;
    }
    if (count != NULL) {
        *count = 0U;
    }
    if (request == NULL || plan == NULL || names == NULL || count == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "pending request, model plan, names, and count are required.");
        return 0;
    }
    if (request->kind != UNITLAB_MMS_REQUEST_GET_NAME_LIST) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "browse response can only be built for GetNameList requests.");
        return 0;
    }

    model_error[0] = '\0';
    int apply_continue_after_filter = 1;

    if (request->browse_object_class == 9U && request->browse_object_scope == 0U) {
        if (!unitlab_collect_ied_model_logical_devices(plan, names, count, model_error, sizeof(model_error))) {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GetNameList logical device browse failed.");
            return 0;
        }
    } else if (request->browse_object_class == 0U && request->browse_object_scope == 1U) {
        if (request->browse_domain_id[0] == '\0') {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList domain-specific browse requires a domain identifier.");
            return 0;
        }
        if (!unitlab_collect_ied_model_logical_device_variables(
                plan,
                request->browse_domain_id,
                names,
                count,
                model_error,
                sizeof(model_error))) {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GetNameList named variable browse failed.");
            return 0;
        }
    } else if (request->browse_object_class == 2U && request->browse_object_scope == 0U) {
        if (!unitlab_collect_ied_model_vmd_named_variable_lists(
                plan,
                names,
                count,
                model_error,
                sizeof(model_error))) {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GetNameList VMD-specific named variable list browse failed.");
            return 0;
        }
    } else if (request->browse_object_class == 2U && request->browse_object_scope == 1U) {
        if (request->browse_domain_id[0] == '\0') {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList domain-specific browse requires a domain identifier.");
            return 0;
        }
        if (!unitlab_collect_ied_model_logical_device_data_sets(
                plan,
                request->browse_domain_id,
                names,
                count,
                model_error,
                sizeof(model_error))) {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GetNameList data set browse failed.");
            return 0;
        }
    } else if (request->browse_object_class == 2U && request->browse_object_scope == 2U) {
        *names = NULL;
        *count = 0U;
    } else if (request->browse_object_class == 1U && request->browse_object_scope == 1U) {
        if (request->browse_domain_id[0] == '\0') {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList logical-node directory browse requires a domain identifier.");
            return 0;
        }
        if (!unitlab_collect_ied_model_logical_node_names(
                plan,
                request->browse_domain_id,
                names,
                count,
                model_error,
                sizeof(model_error))) {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GetNameList logical-node directory browse failed.");
            return 0;
        }
    } else if (request->browse_object_class == 3U && request->browse_object_scope == 1U) {
        const char* node_id = request->browse_node_id[0] != '\0' ? request->browse_node_id : request->browse_continue_after;
        if (request->browse_domain_id[0] == '\0' || node_id[0] == '\0') {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList logical-node contents browse requires a domain and logical node identifier.");
            return 0;
        }
        if (!unitlab_collect_ied_model_logical_node_variables(
                plan,
                request->browse_domain_id,
                node_id,
                names,
                count,
                model_error,
                sizeof(model_error))) {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GetNameList logical-node browse failed.");
            return 0;
        }
        apply_continue_after_filter = request->browse_node_id[0] != '\0' && request->browse_continue_after[0] != '\0';
    } else if (request->browse_object_class == 4U && request->browse_object_scope == 1U) {
        const char* node_id = request->browse_node_id[0] != '\0' ? request->browse_node_id : request->browse_continue_after;
        if (request->browse_domain_id[0] == '\0' || node_id[0] == '\0') {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList buffered report browse requires a domain and logical node identifier.");
            return 0;
        }
        if (!unitlab_collect_ied_model_logical_node_reports(
                plan,
                request->browse_domain_id,
                node_id,
                UNITLAB_IED_MODEL_REPORT_CONTROL_KIND_BUFFERED,
                names,
                count,
                model_error,
                sizeof(model_error))) {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GetNameList buffered report browse failed.");
            return 0;
        }
        apply_continue_after_filter = request->browse_node_id[0] != '\0' && request->browse_continue_after[0] != '\0';
    } else if (request->browse_object_class == 5U && request->browse_object_scope == 1U) {
        const char* node_id = request->browse_node_id[0] != '\0' ? request->browse_node_id : request->browse_continue_after;
        if (request->browse_domain_id[0] == '\0' || node_id[0] == '\0') {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList unbuffered report browse requires a domain and logical node identifier.");
            return 0;
        }
        if (!unitlab_collect_ied_model_logical_node_reports(
                plan,
                request->browse_domain_id,
                node_id,
                UNITLAB_IED_MODEL_REPORT_CONTROL_KIND_UNBUFFERED,
                names,
                count,
                model_error,
                sizeof(model_error))) {
            set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GetNameList unbuffered report browse failed.");
            return 0;
        }
        apply_continue_after_filter = request->browse_node_id[0] != '\0' && request->browse_continue_after[0] != '\0';
    } else {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "GetNameList browse class or scope is unsupported.");
        return 0;
    }

    if (apply_continue_after_filter && !unitlab_mms_filter_browse_continue_after(names, count, request->browse_continue_after, diagnostic)) {
        return 0;
    }
    return 1;
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
        snapshot->report_control_state = report_control->state;
        snapshot->report_control_last_event = report_control->last_event;
        snapshot->report_control_event_log = report_control->event_log;
    }
    if (transport != NULL) {
        snapshot->transport = *transport;
    }
    if (last_result != NULL) {
        snapshot->last_result = *last_result;
    }
}

void unitlab_mms_pending_request_init(UnitLabMmsPendingRequest* request)
{
    if (request == NULL) {
        return;
    }
    memset(request, 0, sizeof(*request));
    request->state = UNITLAB_MMS_PENDING_REQUEST_IDLE;
    runtime_event_clear(&request->last_event);
    runtime_event_log_clear(&request->event_log);
}

int unitlab_mms_pending_request_start(UnitLabMmsPendingRequest* request, UnitLabMmsRequestKind kind, uint32_t invoke_id, uint32_t correlation_id, uint64_t deadline_ms, uint64_t timestamp_ms, UnitLabMmsDiagnostic* diagnostic)
{
    if (request == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "pending request is required to start tracking.");
        return 0;
    }
    if (request->state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_REQUEST_ALREADY_ACTIVE, "pending request is already active.");
        runtime_event_set(&request->last_event, UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED, request->state, request->state, invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_REQUEST_ALREADY_ACTIVE, diagnostic == NULL ? "pending request is already active." : diagnostic->message);
        request->last_event.request_kind = (uint32_t)kind;
        request->last_event.correlation_id = correlation_id;
        request->last_event.timestamp_ms = timestamp_ms;
        request->last_event.deadline_ms = deadline_ms;
        runtime_event_log_append(&request->event_log, &request->last_event);
        return 0;
    }
    request->kind = kind;
    request->state = UNITLAB_MMS_PENDING_REQUEST_ACTIVE;
    request->invoke_id = invoke_id;
    request->correlation_id = correlation_id;
    request->deadline_ms = deadline_ms;
    request->timestamp_ms = timestamp_ms;
    request->timed_out = 0;
    request->completed = 0;
    request->object_reference[0] = '\0';
    request->attribute_reference[0] = '\0';
    request->read_object_reference_count = 0U;
    request->write_object_reference_count = 0U;
    memset(request->write_values, 0, sizeof(request->write_values));
    memset(request->write_value_lengths, 0, sizeof(request->write_value_lengths));
    request->browse_object_class = 0U;
    request->browse_object_scope = 0U;
    request->browse_domain_id[0] = '\0';
    request->browse_node_id[0] = '\0';
    request->browse_continue_after[0] = '\0';
    runtime_event_set(&request->last_event, UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED, 0U, 0U, invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    request->last_event.request_kind = (uint32_t)kind;
    request->last_event.correlation_id = correlation_id;
    request->last_event.timestamp_ms = timestamp_ms;
    request->last_event.deadline_ms = deadline_ms;
    request->last_event.completed = 0;
    request->last_event.timed_out = 0;
    runtime_event_log_append(&request->event_log, &request->last_event);
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_pending_request_complete(UnitLabMmsPendingRequest* request, uint64_t completed_at_ms, UnitLabMmsDiagnostic* diagnostic)
{
    if (request == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "pending request is required to complete tracking.");
        return 0;
    }
    if (request->state != UNITLAB_MMS_PENDING_REQUEST_ACTIVE) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_REQUEST_NOT_ACTIVE, "pending request must be active before completion.");
        runtime_event_set(&request->last_event, UNITLAB_MMS_RUNTIME_EVENT_REQUEST_COMPLETED, request->state, request->state, request->invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_REQUEST_NOT_ACTIVE, diagnostic == NULL ? "pending request must be active before completion." : diagnostic->message);
        request->last_event.request_kind = (uint32_t)request->kind;
        request->last_event.timestamp_ms = completed_at_ms;
        runtime_event_log_append(&request->event_log, &request->last_event);
        return 0;
    }
    request->state = UNITLAB_MMS_PENDING_REQUEST_COMPLETED;
    request->completed = 1;
    request->timed_out = 0;
    runtime_event_set(&request->last_event, UNITLAB_MMS_RUNTIME_EVENT_REQUEST_COMPLETED, UNITLAB_MMS_PENDING_REQUEST_ACTIVE, request->state, request->invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    request->last_event.request_kind = (uint32_t)request->kind;
    request->last_event.timestamp_ms = completed_at_ms;
    request->last_event.completed = 1;
    runtime_event_log_append(&request->event_log, &request->last_event);
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int unitlab_mms_pending_request_mark_timed_out(UnitLabMmsPendingRequest* request, uint64_t timed_out_at_ms, UnitLabMmsDiagnostic* diagnostic)
{
    if (request == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "pending request is required to mark timeout.");
        return 0;
    }
    if (request->state != UNITLAB_MMS_PENDING_REQUEST_ACTIVE) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_REQUEST_NOT_ACTIVE, "pending request must be active before timeout.");
        runtime_event_set(&request->last_event, UNITLAB_MMS_RUNTIME_EVENT_REQUEST_TIMED_OUT, request->state, request->state, request->invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_REQUEST_NOT_ACTIVE, diagnostic == NULL ? "pending request must be active before timeout." : diagnostic->message);
        request->last_event.request_kind = (uint32_t)request->kind;
        request->last_event.timestamp_ms = timed_out_at_ms;
        request->last_event.deadline_ms = request->deadline_ms;
        request->last_event.timed_out = 1;
        runtime_event_log_append(&request->event_log, &request->last_event);
        return 0;
    }
    request->state = UNITLAB_MMS_PENDING_REQUEST_TIMED_OUT;
    request->timed_out = 1;
    request->completed = 0;
    runtime_event_set(&request->last_event, UNITLAB_MMS_RUNTIME_EVENT_REQUEST_TIMED_OUT, UNITLAB_MMS_PENDING_REQUEST_ACTIVE, request->state, request->invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_TIMEOUT, NULL);
    request->last_event.request_kind = (uint32_t)request->kind;
    request->last_event.timestamp_ms = timed_out_at_ms;
    request->last_event.deadline_ms = request->deadline_ms;
    request->last_event.completed = 0;
    request->last_event.timed_out = 1;
    runtime_event_log_append(&request->event_log, &request->last_event);
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_TIMEOUT, NULL);
    return 1;
}

void unitlab_mms_associate_request_init(UnitLabMmsAssociateRequest* request)
{
    if (request == NULL) {
        return;
    }
    memset(request, 0, sizeof(*request));
}

void unitlab_mms_read_request_init(UnitLabMmsReadRequest* request)
{
    if (request == NULL) {
        return;
    }
    memset(request, 0, sizeof(*request));
}

void unitlab_mms_write_request_init(UnitLabMmsWriteRequest* request)
{
    if (request == NULL) {
        return;
    }
    memset(request, 0, sizeof(*request));
}

void unitlab_mms_information_report_init(UnitLabMmsInformationReport* report)
{
    if (report == NULL) {
        return;
    }
    memset(report, 0, sizeof(*report));
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

int unitlab_mms_session_complete_release(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic)
{
    if (session == NULL) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required for release completion.");
        return 0;
    }
    if (session->state != UNITLAB_MMS_SESSION_RELEASING) {
        set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "release can only complete from releasing state.");
        runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_RELEASED, session->state, session->state, session->active_invoke_id, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, diagnostic == NULL ? "release can only complete from releasing state." : diagnostic->message);
        return 0;
    }
    UnitLabMmsSessionState before = session->state;
    session->state = UNITLAB_MMS_SESSION_DISCONNECTED;
    session->active_invoke_id = 0U;
    set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    runtime_event_set_and_append(&session->last_event, &session->event_log, UNITLAB_MMS_RUNTIME_EVENT_SESSION_RELEASED, before, session->state, 0U, 0U, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
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

int unitlab_mms_runtime_apply_semantic_result(UnitLabMmsSession* session, UnitLabMmsPendingRequest* pending_request, const UnitLabMmsSemanticResult* semantic_result, UnitLabMmsOperationResult* operation_result)
{
    if (operation_result == NULL) {
        return 0;
    }
    unitlab_mms_operation_result_init(operation_result);
    if (semantic_result == NULL) {
        set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "semantic result is required.");
        operation_result->ok = 0;
        return 0;
    }
    operation_result->diagnostic = semantic_result->diagnostic.diagnostic;
    if (!semantic_result->ok) {
        operation_result->ok = 0;
        return 0;
    }

    switch (semantic_result->pdu.kind) {
        case UNITLAB_MMS_DECODED_PDU_ASSOCIATE_RESPONSE:
            if (session == NULL) {
                set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required to apply associate response.");
                operation_result->ok = 0;
                return 0;
            }
            uint32_t association_invoke_id = semantic_result->pdu.invoke_id;
            if (association_invoke_id == 0U) {
                association_invoke_id = session->active_invoke_id;
            }
            operation_result->ok = unitlab_mms_session_complete_association(session, association_invoke_id, &operation_result->diagnostic);
            operation_result_project_from_runtime(operation_result, operation_result->ok, &operation_result->diagnostic, &session->event_log, &session->last_event);
            return operation_result->ok;
        case UNITLAB_MMS_DECODED_PDU_ASSOCIATE_REQUEST:
            if (session == NULL) {
                set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required to apply associate request.");
                operation_result->ok = 0;
                return 0;
            }
            operation_result->ok = unitlab_mms_session_begin_association(session, &operation_result->diagnostic);
            operation_result_project_from_runtime(operation_result, operation_result->ok, &operation_result->diagnostic, &session->event_log, &session->last_event);
            return operation_result->ok;
        case UNITLAB_MMS_DECODED_PDU_RELEASE_REQUEST:
            if (session == NULL) {
                set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required to apply release request.");
                operation_result->ok = 0;
                return 0;
            }
            operation_result->ok = unitlab_mms_session_begin_release(session, &operation_result->diagnostic);
            operation_result_project_from_runtime(operation_result, operation_result->ok, &operation_result->diagnostic, &session->event_log, &session->last_event);
            return operation_result->ok;
        case UNITLAB_MMS_DECODED_PDU_RELEASE_RESPONSE:
            if (session == NULL) {
                set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required to apply release response.");
                operation_result->ok = 0;
                return 0;
            }
            operation_result->ok = unitlab_mms_session_complete_release(session, &operation_result->diagnostic);
            operation_result_project_from_runtime(operation_result, operation_result->ok, &operation_result->diagnostic, &session->event_log, &session->last_event);
            return operation_result->ok;
        case UNITLAB_MMS_DECODED_PDU_READ_REQUEST:
        case UNITLAB_MMS_DECODED_PDU_WRITE_REQUEST:
        case UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_REQUEST:
        case UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_REQUEST:
        case UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_REQUEST:
            if (pending_request == NULL) {
                set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "pending request is required to apply confirmed requests.");
                operation_result->ok = 0;
                return 0;
            }
            operation_result->ok = unitlab_mms_pending_request_start(
                pending_request,
                semantic_result->pdu.kind == UNITLAB_MMS_DECODED_PDU_READ_REQUEST
                    ? UNITLAB_MMS_REQUEST_READ
                    : semantic_result->pdu.kind == UNITLAB_MMS_DECODED_PDU_WRITE_REQUEST
                        ? UNITLAB_MMS_REQUEST_WRITE
                        : semantic_result->pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_REQUEST
                            ? UNITLAB_MMS_REQUEST_GET_NAME_LIST
                            : semantic_result->pdu.kind == UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_REQUEST
                                ? UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES
                                : UNITLAB_MMS_REQUEST_GET_NAMED_VARIABLE_LIST_ATTRIBUTES,
                semantic_result->pdu.invoke_id,
                semantic_result->pdu.correlation_id,
                semantic_result->pdu.deadline_ms,
                semantic_result->pdu.timestamp_ms,
                &operation_result->diagnostic);
            if (operation_result->ok) {
                snprintf(pending_request->object_reference, sizeof(pending_request->object_reference), "%s", semantic_result->pdu.object_reference);
                snprintf(pending_request->attribute_reference, sizeof(pending_request->attribute_reference), "%s", semantic_result->pdu.attribute_reference);
                pending_request->browse_object_class = semantic_result->pdu.object_class;
                pending_request->browse_object_scope = semantic_result->pdu.object_scope;
                snprintf(pending_request->browse_domain_id, sizeof(pending_request->browse_domain_id), "%s", semantic_result->pdu.domain_id);
                snprintf(pending_request->browse_node_id, sizeof(pending_request->browse_node_id), "%s", semantic_result->pdu.node_id);
                snprintf(pending_request->browse_continue_after, sizeof(pending_request->browse_continue_after), "%s", semantic_result->pdu.continue_after);
                pending_request->write_value_length = 0U;
                pending_request->write_object_reference_count = 0U;
                memset(pending_request->write_values, 0, sizeof(pending_request->write_values));
                memset(pending_request->write_value_lengths, 0, sizeof(pending_request->write_value_lengths));
                if (semantic_result->pdu.kind == UNITLAB_MMS_DECODED_PDU_WRITE_REQUEST) {
                    if (semantic_result->pdu.value_bytes != NULL && semantic_result->pdu.value_length <= sizeof(pending_request->write_value)) {
                        memcpy(pending_request->write_value, semantic_result->pdu.value_bytes, semantic_result->pdu.value_length);
                        pending_request->write_value_length = semantic_result->pdu.value_length;
                    }
                    pending_request->write_object_reference_count = semantic_result->pdu.write_object_reference_count;
                    if (pending_request->write_object_reference_count > UNITLAB_MMS_MAX_READ_VARIABLES) {
                        pending_request->write_object_reference_count = UNITLAB_MMS_MAX_READ_VARIABLES;
                    }
                    for (size_t index = 0U; index < pending_request->write_object_reference_count; index++) {
                        snprintf(pending_request->write_object_references[index], sizeof(pending_request->write_object_references[index]), "%s", semantic_result->pdu.write_object_references[index]);
                        snprintf(pending_request->write_attribute_references[index], sizeof(pending_request->write_attribute_references[index]), "%s", semantic_result->pdu.write_attribute_references[index]);
                        pending_request->write_value_lengths[index] = semantic_result->pdu.write_value_lengths[index];
                        if (pending_request->write_value_lengths[index] > sizeof(pending_request->write_values[index])) {
                            pending_request->write_value_lengths[index] = sizeof(pending_request->write_values[index]);
                        }
                        memcpy(pending_request->write_values[index], semantic_result->pdu.write_values[index], pending_request->write_value_lengths[index]);
                    }
                }
                if (semantic_result->pdu.kind == UNITLAB_MMS_DECODED_PDU_READ_REQUEST) {
                    pending_request->read_object_reference_count = semantic_result->pdu.read_object_reference_count;
                    if (pending_request->read_object_reference_count > UNITLAB_MMS_MAX_READ_VARIABLES) {
                        pending_request->read_object_reference_count = UNITLAB_MMS_MAX_READ_VARIABLES;
                    }
                    for (size_t index = 0U; index < pending_request->read_object_reference_count; index++) {
                        snprintf(pending_request->read_object_references[index], sizeof(pending_request->read_object_references[index]), "%s", semantic_result->pdu.read_object_references[index]);
                        snprintf(pending_request->read_attribute_references[index], sizeof(pending_request->read_attribute_references[index]), "%s", semantic_result->pdu.read_attribute_references[index]);
                    }
                }
            }
            operation_result_project_from_runtime(operation_result, operation_result->ok, &operation_result->diagnostic, &pending_request->event_log, &pending_request->last_event);
            return operation_result->ok;
        case UNITLAB_MMS_DECODED_PDU_READ_RESPONSE:
        case UNITLAB_MMS_DECODED_PDU_WRITE_RESPONSE:
        case UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_RESPONSE:
        case UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_RESPONSE:
        case UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_RESPONSE:
            if (pending_request == NULL) {
                set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "pending request is required to apply confirmed responses.");
                operation_result->ok = 0;
                return 0;
            }
            if (pending_request->invoke_id != semantic_result->pdu.invoke_id) {
                UnitLabMmsRuntimeEvent mismatch_event;
                UnitLabMmsRuntimeEventLog mismatch_trace;

                set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVOKE_ID_MISMATCH, "response invoke id does not match pending request.");
                unitlab_mms_runtime_event_init(&mismatch_event);
                unitlab_mms_runtime_event_log_init(&mismatch_trace);
                mismatch_event.kind = UNITLAB_MMS_RUNTIME_EVENT_REQUEST_CORRELATION_MISMATCH;
                mismatch_event.invoke_id = semantic_result->pdu.invoke_id;
                mismatch_event.correlation_id = pending_request->correlation_id;
                mismatch_event.state_before = (uint32_t)pending_request->state;
                mismatch_event.state_after = (uint32_t)pending_request->state;
                mismatch_event.diagnostic_code = UNITLAB_MMS_DIAGNOSTIC_INVOKE_ID_MISMATCH;
                strncpy(mismatch_event.diagnostic_message, operation_result->diagnostic.message, sizeof(mismatch_event.diagnostic_message) - 1U);
                mismatch_event.diagnostic_message[sizeof(mismatch_event.diagnostic_message) - 1U] = '\0';
                mismatch_trace.events[0] = mismatch_event;
                mismatch_trace.count = 1U;
                operation_result->ok = 0;
                operation_result_project_from_runtime(operation_result, 0, &operation_result->diagnostic, &mismatch_trace, &mismatch_trace.events[0]);
                return 0;
            }
            operation_result->ok = unitlab_mms_pending_request_complete(pending_request, semantic_result->pdu.timestamp_ms, &operation_result->diagnostic);
            operation_result_project_from_runtime(operation_result, operation_result->ok, &operation_result->diagnostic, &pending_request->event_log, &pending_request->last_event);
            return operation_result->ok;
        case UNITLAB_MMS_DECODED_PDU_ABORT:
            if (session == NULL) {
                set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "session is required to apply abort.");
                operation_result->ok = 0;
                return 0;
            }
            operation_result->ok = unitlab_mms_session_abort(session, &operation_result->diagnostic);
            operation_result_project_from_runtime(operation_result, operation_result->ok, &operation_result->diagnostic, &session->event_log, &session->last_event);
            return operation_result->ok;
        case UNITLAB_MMS_DECODED_PDU_INFORMATION_REPORT:
        {
            UnitLabMmsRuntimeEvent event;
            UnitLabMmsRuntimeEventLog trace;

            unitlab_mms_runtime_event_init(&event);
            unitlab_mms_runtime_event_log_init(&trace);
            event.kind = UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED;
            event.invoke_id = semantic_result->pdu.invoke_id;
            event.correlation_id = semantic_result->pdu.correlation_id;
            event.timestamp_ms = semantic_result->pdu.timestamp_ms;
            event.deadline_ms = semantic_result->pdu.deadline_ms;
            event.diagnostic_code = UNITLAB_MMS_DIAGNOSTIC_OK;
            trace.events[0] = event;
            trace.count = 1U;
            operation_result->ok = 1;
            operation_result_project_from_runtime(operation_result, 1, &operation_result->diagnostic, &trace, &trace.events[0]);
            return 1;
        }
        case UNITLAB_MMS_DECODED_PDU_REJECT:
            operation_result->reject = semantic_result->pdu.reject;
            set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "semantic reject requires a wire-layer reject handler.");
            operation_result->ok = 0;
            return 0;
        default:
            set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported semantic PDU kind.");
            operation_result->ok = 0;
            return 0;
    }
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
