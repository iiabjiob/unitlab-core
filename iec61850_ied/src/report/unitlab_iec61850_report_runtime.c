#include "report/unitlab_iec61850_report_runtime.h"

#include <string.h>

static void report_runtime_event_set(UnitLabMmsRuntimeEvent* event, UnitLabMmsRuntimeEventKind kind, uint32_t state_before, uint32_t state_after, uint32_t invoke_id, UnitLabMmsDiagnosticCode diagnostic_code, const char* diagnostic_message)
{
    if (event == NULL) {
        return;
    }
    unitlab_mms_runtime_event_init(event);
    event->kind = kind;
    event->state_before = state_before;
    event->state_after = state_after;
    event->invoke_id = invoke_id;
    event->diagnostic_code = diagnostic_code;
    if (diagnostic_message != NULL) {
        strncpy(event->diagnostic_message, diagnostic_message, sizeof(event->diagnostic_message) - 1U);
        event->diagnostic_message[sizeof(event->diagnostic_message) - 1U] = '\0';
    }
}

static void report_runtime_event_append(UnitLabIec61850ReportControl* report_control, const UnitLabMmsRuntimeEvent* event)
{
    if (report_control == NULL || event == NULL) {
        return;
    }
    if (report_control->event_log.count >= UNITLAB_MMS_RUNTIME_EVENT_LOG_CAPACITY) {
        memmove(&report_control->event_log.events[0], &report_control->event_log.events[1], sizeof(report_control->event_log.events[0]) * (UNITLAB_MMS_RUNTIME_EVENT_LOG_CAPACITY - 1U));
        report_control->event_log.count = UNITLAB_MMS_RUNTIME_EVENT_LOG_CAPACITY - 1U;
    }
    report_control->event_log.events[report_control->event_log.count] = *event;
    report_control->event_log.count++;
}

static void report_runtime_diagnostic_clear(UnitLabMmsDiagnostic* diagnostic)
{
    unitlab_mms_diagnostic_clear(diagnostic);
}

static int report_control_fail(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, UnitLabMmsRuntimeEventKind kind, const char* message)
{
    if (report_control != NULL) {
        report_runtime_event_set(&report_control->last_event, kind, report_control->state, report_control->state, 0U, code, message);
        report_runtime_event_append(report_control, &report_control->last_event);
    }
    if (diagnostic != NULL) {
        diagnostic->code = code;
        if (message == NULL) {
            diagnostic->message[0] = '\0';
        } else {
            strncpy(diagnostic->message, message, sizeof(diagnostic->message) - 1U);
            diagnostic->message[sizeof(diagnostic->message) - 1U] = '\0';
        }
    }
    return 0;
}

void unitlab_iec61850_report_control_init(UnitLabIec61850ReportControl* report_control)
{
    if (report_control == NULL) {
        return;
    }
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_DISABLED;
    unitlab_mms_runtime_event_init(&report_control->last_event);
    unitlab_mms_runtime_event_log_init(&report_control->event_log);
}

void unitlab_iec61850_report_control_reset(UnitLabIec61850ReportControl* report_control)
{
    unitlab_iec61850_report_control_init(report_control);
}

int unitlab_iec61850_report_control_reserve(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE, "report control is required for reserve.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_RCB_NOT_DISABLED, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE, "reserve requires a disabled report control.");
    }
    UnitLabIec61850ReportControlState before = report_control->state;
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_RESERVED;
    report_runtime_diagnostic_clear(diagnostic);
    report_runtime_event_set(&report_control->last_event, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE, before, report_control->state, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    report_runtime_event_append(report_control, &report_control->last_event);
    return 1;
}

int unitlab_iec61850_report_control_enable(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE, "report control is required for enable.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_RESERVED) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_RCB_NOT_RESERVED, UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE, "enable requires a reserved report control.");
    }
    UnitLabIec61850ReportControlState before = report_control->state;
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_ENABLED;
    report_runtime_diagnostic_clear(diagnostic);
    report_runtime_event_set(&report_control->last_event, UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE, before, report_control->state, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    report_runtime_event_append(report_control, &report_control->last_event);
    return 1;
}

int unitlab_iec61850_report_control_request_gi(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI, "report control is required for GI.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_ENABLED) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_RCB_NOT_ENABLED, UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI, "GI requires an enabled report control.");
    }
    UnitLabIec61850ReportControlState before = report_control->state;
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING;
    report_runtime_diagnostic_clear(diagnostic);
    report_runtime_event_set(&report_control->last_event, UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI, before, report_control->state, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    report_runtime_event_append(report_control, &report_control->last_event);
    return 1;
}

int unitlab_iec61850_report_control_disable(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE, "report control is required for disable.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_ENABLED && report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING && report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_REPORTING) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE, "disable requires an enabled report control.");
    }
    UnitLabIec61850ReportControlState before = report_control->state;
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_DISABLED;
    report_runtime_diagnostic_clear(diagnostic);
    report_runtime_event_set(&report_control->last_event, UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE, before, report_control->state, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    report_runtime_event_append(report_control, &report_control->last_event);
    return 1;
}

int unitlab_iec61850_report_control_accept_report(UnitLabIec61850ReportControl* report_control, uint32_t invoke_id, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED, "report control is required to accept a report.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED, "report acceptance requires a GI-pending report control.");
    }
    UnitLabIec61850ReportControlState before = report_control->state;
    report_control->state = UNITLAB_IEC61850_REPORT_CONTROL_REPORTING;
    report_runtime_diagnostic_clear(diagnostic);
    report_runtime_event_set(&report_control->last_event, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED, before, report_control->state, invoke_id, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    report_runtime_event_append(report_control, &report_control->last_event);
    return 1;
}

int unitlab_iec61850_report_control_release(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic)
{
    if (report_control == NULL) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE, "report control is required for release.");
    }
    if (report_control->state != UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
        return report_control_fail(report_control, diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE, "release requires a disabled report control.");
    }
    report_runtime_diagnostic_clear(diagnostic);
    report_runtime_event_set(&report_control->last_event, UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE, report_control->state, report_control->state, 0U, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    report_runtime_event_append(report_control, &report_control->last_event);
    return 1;
}
