#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_TYPES_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef enum UnitLabMmsDiagnosticCode {
    UNITLAB_MMS_DIAGNOSTIC_OK = 0,
    UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT = 1,
    UNITLAB_MMS_DIAGNOSTIC_BAD_STATE = 2,
    UNITLAB_MMS_DIAGNOSTIC_NOT_ASSOCIATED = 3,
    UNITLAB_MMS_DIAGNOSTIC_TIMEOUT = 4,
    UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR = 5,
    UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED = 6,
    UNITLAB_MMS_DIAGNOSTIC_INVOKE_ID_MISMATCH = 7,
    UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL = 8,
    UNITLAB_MMS_DIAGNOSTIC_RESPONSE_NOT_BOUND = 9,
    UNITLAB_MMS_DIAGNOSTIC_REQUEST_ALREADY_ACTIVE = 10,
    UNITLAB_MMS_DIAGNOSTIC_REQUEST_NOT_ACTIVE = 11,
    UNITLAB_MMS_DIAGNOSTIC_RCB_NOT_RESERVED = 12,
    UNITLAB_MMS_DIAGNOSTIC_RCB_NOT_ENABLED = 13
} UnitLabMmsDiagnosticCode;

typedef struct UnitLabMmsDiagnostic {
    UnitLabMmsDiagnosticCode code;
    char message[256];
} UnitLabMmsDiagnostic;

typedef enum UnitLabMmsRuntimeEventKind {
    UNITLAB_MMS_RUNTIME_EVENT_NONE = 0,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION = 1,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION = 2,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_RELEASE = 3,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_ABORT = 4,
    UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED = 5,
    UNITLAB_MMS_RUNTIME_EVENT_REQUEST_SENT = 6,
    UNITLAB_MMS_RUNTIME_EVENT_REQUEST_BOUND = 7,
    UNITLAB_MMS_RUNTIME_EVENT_REQUEST_COMPLETED = 8,
    UNITLAB_MMS_RUNTIME_EVENT_REQUEST_TIMED_OUT = 9,
    UNITLAB_MMS_RUNTIME_EVENT_RCB_RESERVED = 10,
    UNITLAB_MMS_RUNTIME_EVENT_RCB_ENABLED = 11,
    UNITLAB_MMS_RUNTIME_EVENT_RCB_DISABLED = 12,
    UNITLAB_MMS_RUNTIME_EVENT_RCB_RELEASED = 13,
    UNITLAB_MMS_RUNTIME_EVENT_GI_REQUESTED = 14,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_RECEIVED = 15,
    UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST = UNITLAB_MMS_RUNTIME_EVENT_REQUEST_BOUND,
    UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_RESPONSE = UNITLAB_MMS_RUNTIME_EVENT_REQUEST_COMPLETED,
    UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH = UNITLAB_MMS_RUNTIME_EVENT_REQUEST_SENT,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE = UNITLAB_MMS_RUNTIME_EVENT_RCB_RESERVED,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE = UNITLAB_MMS_RUNTIME_EVENT_RCB_ENABLED,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI = UNITLAB_MMS_RUNTIME_EVENT_GI_REQUESTED,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE = UNITLAB_MMS_RUNTIME_EVENT_RCB_DISABLED,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE = UNITLAB_MMS_RUNTIME_EVENT_RCB_RELEASED
} UnitLabMmsRuntimeEventKind;

typedef struct UnitLabMmsRuntimeEvent {
    UnitLabMmsRuntimeEventKind kind;
    uint32_t request_kind;
    uint32_t state_before;
    uint32_t state_after;
    uint32_t invoke_id;
    uint32_t correlation_id;
    uint64_t timestamp_ms;
    uint64_t deadline_ms;
    size_t request_length;
    size_t response_length;
    int timed_out;
    int completed;
    UnitLabMmsDiagnosticCode diagnostic_code;
    char diagnostic_message[256];
} UnitLabMmsRuntimeEvent;

#define UNITLAB_MMS_RUNTIME_EVENT_LOG_CAPACITY 8U

typedef struct UnitLabMmsRuntimeEventLog {
    size_t count;
    UnitLabMmsRuntimeEvent events[UNITLAB_MMS_RUNTIME_EVENT_LOG_CAPACITY];
} UnitLabMmsRuntimeEventLog;

typedef struct UnitLabMmsOperationResult {
    int ok;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsRuntimeEvent event;
    UnitLabMmsRuntimeEventLog trace;
} UnitLabMmsOperationResult;

void unitlab_mms_diagnostic_clear(UnitLabMmsDiagnostic* diagnostic);
void unitlab_mms_runtime_event_init(UnitLabMmsRuntimeEvent* event);
void unitlab_mms_runtime_event_log_init(UnitLabMmsRuntimeEventLog* event_log);
size_t unitlab_mms_runtime_event_log_count(const UnitLabMmsRuntimeEventLog* event_log);
const UnitLabMmsRuntimeEvent* unitlab_mms_runtime_event_log_at(const UnitLabMmsRuntimeEventLog* event_log, size_t index);
void unitlab_mms_operation_result_init(UnitLabMmsOperationResult* result);
void unitlab_mms_operation_result_from_event(UnitLabMmsOperationResult* result, int ok, const UnitLabMmsDiagnostic* diagnostic, const UnitLabMmsRuntimeEvent* event);
void unitlab_mms_operation_result_from_trace(UnitLabMmsOperationResult* result, int ok, const UnitLabMmsDiagnostic* diagnostic, const UnitLabMmsRuntimeEventLog* trace, const UnitLabMmsRuntimeEvent* event);

#endif
