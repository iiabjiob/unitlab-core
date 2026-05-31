#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_CORE_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_CORE_H

#include <stddef.h>
#include <stdint.h>

typedef enum UnitLabMmsDiagnosticCode {
    UNITLAB_MMS_DIAGNOSTIC_OK = 0,
    UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT = 1,
    UNITLAB_MMS_DIAGNOSTIC_NOT_ASSOCIATED = 2,
    UNITLAB_MMS_DIAGNOSTIC_TIMEOUT = 3,
    UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR = 4,
    UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED = 5
} UnitLabMmsDiagnosticCode;

typedef struct UnitLabMmsDiagnostic {
    UnitLabMmsDiagnosticCode code;
    char message[256];
} UnitLabMmsDiagnostic;

typedef enum UnitLabMmsSessionState {
    UNITLAB_MMS_SESSION_DISCONNECTED = 0,
    UNITLAB_MMS_SESSION_ASSOCIATING = 1,
    UNITLAB_MMS_SESSION_ASSOCIATED = 2,
    UNITLAB_MMS_SESSION_RELEASING = 3,
    UNITLAB_MMS_SESSION_ABORTED = 4
} UnitLabMmsSessionState;

typedef enum UnitLabMmsRuntimeEventKind {
    UNITLAB_MMS_RUNTIME_EVENT_NONE = 0,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION = 1,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION = 2,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_RELEASE = 3,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_ABORT = 4,
    UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST = 5,
    UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_RESPONSE = 6,
    UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH = 7,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE = 8,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE = 9,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI = 10,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE = 11,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE = 12
} UnitLabMmsRuntimeEventKind;

typedef struct UnitLabMmsRuntimeEvent {
    UnitLabMmsRuntimeEventKind kind;
    uint32_t state_before;
    uint32_t state_after;
    uint32_t invoke_id;
    size_t request_length;
    size_t response_length;
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
} UnitLabMmsOperationResult;

typedef struct UnitLabMmsSession {
    UnitLabMmsSessionState state;
    uint32_t next_invoke_id;
    uint32_t active_invoke_id;
    UnitLabMmsRuntimeEvent last_event;
    UnitLabMmsRuntimeEventLog event_log;
} UnitLabMmsSession;

typedef enum UnitLabIec61850ReportControlState {
    UNITLAB_IEC61850_REPORT_CONTROL_DISABLED = 0,
    UNITLAB_IEC61850_REPORT_CONTROL_RESERVED = 1,
    UNITLAB_IEC61850_REPORT_CONTROL_ENABLED = 2,
    UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING = 3,
    UNITLAB_IEC61850_REPORT_CONTROL_REPORTING = 4
} UnitLabIec61850ReportControlState;

typedef struct UnitLabIec61850ReportControl {
    UnitLabIec61850ReportControlState state;
    UnitLabMmsRuntimeEvent last_event;
    UnitLabMmsRuntimeEventLog event_log;
} UnitLabIec61850ReportControl;

typedef struct UnitLabMmsTransportExchange {
    const uint8_t* request_bytes;
    size_t request_length;
    uint8_t* response_bytes;
    size_t response_capacity;
    size_t response_length;
    uint32_t invoke_id;
    UnitLabMmsRuntimeEvent last_event;
    UnitLabMmsRuntimeEventLog event_log;
} UnitLabMmsTransportExchange;

void unitlab_mms_diagnostic_clear(UnitLabMmsDiagnostic* diagnostic);
void unitlab_mms_runtime_event_init(UnitLabMmsRuntimeEvent* event);
void unitlab_mms_runtime_event_log_init(UnitLabMmsRuntimeEventLog* event_log);
size_t unitlab_mms_runtime_event_log_count(const UnitLabMmsRuntimeEventLog* event_log);
const UnitLabMmsRuntimeEvent* unitlab_mms_runtime_event_log_at(const UnitLabMmsRuntimeEventLog* event_log, size_t index);
void unitlab_mms_operation_result_init(UnitLabMmsOperationResult* result);
void unitlab_mms_operation_result_from_event(UnitLabMmsOperationResult* result, int ok, const UnitLabMmsDiagnostic* diagnostic, const UnitLabMmsRuntimeEvent* event);
void unitlab_mms_session_init(UnitLabMmsSession* session);
void unitlab_mms_session_reset(UnitLabMmsSession* session);
uint32_t unitlab_mms_session_next_invoke_id(UnitLabMmsSession* session);
int unitlab_mms_session_begin_association(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_session_complete_association(UnitLabMmsSession* session, uint32_t invoke_id, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_session_begin_release(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_session_abort(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_session_is_associated(const UnitLabMmsSession* session);
void unitlab_iec61850_report_control_init(UnitLabIec61850ReportControl* report_control);
void unitlab_iec61850_report_control_reset(UnitLabIec61850ReportControl* report_control);
int unitlab_iec61850_report_control_reserve(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);
int unitlab_iec61850_report_control_enable(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);
int unitlab_iec61850_report_control_request_gi(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);
int unitlab_iec61850_report_control_disable(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);
int unitlab_iec61850_report_control_release(UnitLabIec61850ReportControl* report_control, UnitLabMmsDiagnostic* diagnostic);
void unitlab_mms_transport_exchange_init(UnitLabMmsTransportExchange* exchange);
int unitlab_mms_transport_exchange_bind_request(UnitLabMmsTransportExchange* exchange, const uint8_t* request_bytes, size_t request_length, uint32_t invoke_id, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_transport_exchange_bind_response(UnitLabMmsTransportExchange* exchange, uint8_t* response_bytes, size_t response_capacity, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_transport_exchange_set_response_length(UnitLabMmsTransportExchange* exchange, size_t response_length, UnitLabMmsDiagnostic* diagnostic);

#endif
