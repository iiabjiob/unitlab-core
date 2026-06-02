#include "protocols/mms/unitlab_mms_types.h"

#include <string.h>

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
