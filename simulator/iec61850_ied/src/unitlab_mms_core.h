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

typedef enum UnitLabMmsRequestKind {
    UNITLAB_MMS_REQUEST_NONE = 0,
    UNITLAB_MMS_REQUEST_ASSOCIATE = 1,
    UNITLAB_MMS_REQUEST_READ = 2,
    UNITLAB_MMS_REQUEST_WRITE = 3,
    UNITLAB_MMS_REQUEST_INFORMATION_REPORT = 4,
    UNITLAB_MMS_REQUEST_RCB_RESERVE = 5,
    UNITLAB_MMS_REQUEST_RCB_ENABLE = 6,
    UNITLAB_MMS_REQUEST_RCB_DISABLE = 7,
    UNITLAB_MMS_REQUEST_RCB_RELEASE = 8,
    UNITLAB_MMS_REQUEST_GI = 9
} UnitLabMmsRequestKind;

typedef enum UnitLabMmsEventKind {
    UNITLAB_MMS_EVENT_NONE = 0,
    UNITLAB_MMS_EVENT_ASSOCIATION_REQUESTED = 1,
    UNITLAB_MMS_EVENT_ASSOCIATION_OPENED = 2,
    UNITLAB_MMS_EVENT_ASSOCIATION_RELEASED = 3,
    UNITLAB_MMS_EVENT_ASSOCIATION_ABORTED = 4,
    UNITLAB_MMS_EVENT_RCB_RESERVED = 5,
    UNITLAB_MMS_EVENT_RCB_ENABLED = 6,
    UNITLAB_MMS_EVENT_RCB_DISABLED = 7,
    UNITLAB_MMS_EVENT_RCB_RELEASED = 8,
    UNITLAB_MMS_EVENT_GI_REQUESTED = 9,
    UNITLAB_MMS_EVENT_REPORT_RECEIVED = 10,
    UNITLAB_MMS_EVENT_REQUEST_BOUND = 11,
    UNITLAB_MMS_EVENT_REQUEST_COMPLETED = 12,
    UNITLAB_MMS_EVENT_REQUEST_TIMED_OUT = 13,
    UNITLAB_MMS_EVENT_TIMEOUT_EXPIRED = 14,
    UNITLAB_MMS_RUNTIME_EVENT_NONE = UNITLAB_MMS_EVENT_NONE,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION = UNITLAB_MMS_EVENT_ASSOCIATION_REQUESTED,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_COMPLETE_ASSOCIATION = UNITLAB_MMS_EVENT_ASSOCIATION_OPENED,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_RELEASE = UNITLAB_MMS_EVENT_ASSOCIATION_RELEASED,
    UNITLAB_MMS_RUNTIME_EVENT_SESSION_ABORT = UNITLAB_MMS_EVENT_ASSOCIATION_ABORTED,
    UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST = UNITLAB_MMS_EVENT_REQUEST_BOUND,
    UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_RESPONSE = UNITLAB_MMS_EVENT_REQUEST_COMPLETED,
    UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH = UNITLAB_MMS_EVENT_REQUEST_COMPLETED,
    UNITLAB_MMS_RUNTIME_EVENT_REQUEST_COMPLETED = UNITLAB_MMS_EVENT_REQUEST_COMPLETED,
    UNITLAB_MMS_RUNTIME_EVENT_REQUEST_TIMED_OUT = UNITLAB_MMS_EVENT_REQUEST_TIMED_OUT,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_RESERVE = UNITLAB_MMS_EVENT_RCB_RESERVED,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_ENABLE = UNITLAB_MMS_EVENT_RCB_ENABLED,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_REQUEST_GI = UNITLAB_MMS_EVENT_GI_REQUESTED,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_DISABLE = UNITLAB_MMS_EVENT_RCB_DISABLED,
    UNITLAB_MMS_RUNTIME_EVENT_REPORT_RELEASE = UNITLAB_MMS_EVENT_RCB_RELEASED
} UnitLabMmsEventKind;

typedef UnitLabMmsEventKind UnitLabMmsRuntimeEventKind;

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


typedef enum UnitLabMmsPendingRequestState {
    UNITLAB_MMS_PENDING_REQUEST_IDLE = 0,
    UNITLAB_MMS_PENDING_REQUEST_ACTIVE = 1,
    UNITLAB_MMS_PENDING_REQUEST_COMPLETED = 2,
    UNITLAB_MMS_PENDING_REQUEST_TIMED_OUT = 3
} UnitLabMmsPendingRequestState;

typedef struct UnitLabMmsPendingRequest {
    UnitLabMmsRequestKind kind;
    UnitLabMmsPendingRequestState state;
    uint32_t invoke_id;
    uint32_t correlation_id;
    uint64_t deadline_ms;
    uint64_t timestamp_ms;
    int timed_out;
    int completed;
    UnitLabMmsRuntimeEvent last_event;
} UnitLabMmsPendingRequest;

typedef struct UnitLabMmsAssociateRequest {
    uint32_t invoke_id;
    uint32_t correlation_id;
    uint64_t deadline_ms;
    char calling_ae_title[64];
    char called_ae_title[64];
} UnitLabMmsAssociateRequest;

typedef struct UnitLabMmsReadRequest {
    uint32_t invoke_id;
    uint32_t correlation_id;
    uint64_t deadline_ms;
    char object_reference[128];
    char attribute_reference[64];
} UnitLabMmsReadRequest;

typedef struct UnitLabMmsWriteRequest {
    uint32_t invoke_id;
    uint32_t correlation_id;
    uint64_t deadline_ms;
    char object_reference[128];
    char attribute_reference[64];
    const uint8_t* value_bytes;
    size_t value_length;
} UnitLabMmsWriteRequest;

typedef struct UnitLabMmsInformationReport {
    uint32_t invoke_id;
    uint32_t correlation_id;
    uint64_t timestamp_ms;
    char report_control_reference[128];
    char data_set_reference[128];
    size_t item_count;
    int buffered;
} UnitLabMmsInformationReport;

typedef struct UnitLabMmsOperationResult {
    int ok;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsRuntimeEvent event;
    UnitLabMmsRuntimeEventLog trace;
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

typedef struct UnitLabMmsRuntimeSnapshot {
    UnitLabMmsSession session;
    UnitLabIec61850ReportControl report_control;
    UnitLabMmsTransportExchange transport;
    UnitLabMmsOperationResult last_result;
} UnitLabMmsRuntimeSnapshot;

void unitlab_mms_diagnostic_clear(UnitLabMmsDiagnostic* diagnostic);
void unitlab_mms_runtime_event_init(UnitLabMmsRuntimeEvent* event);
void unitlab_mms_runtime_event_log_init(UnitLabMmsRuntimeEventLog* event_log);
size_t unitlab_mms_runtime_event_log_count(const UnitLabMmsRuntimeEventLog* event_log);
const UnitLabMmsRuntimeEvent* unitlab_mms_runtime_event_log_at(const UnitLabMmsRuntimeEventLog* event_log, size_t index);
void unitlab_mms_pending_request_init(UnitLabMmsPendingRequest* request);
int unitlab_mms_pending_request_start(UnitLabMmsPendingRequest* request, UnitLabMmsRequestKind kind, uint32_t invoke_id, uint32_t correlation_id, uint64_t deadline_ms, uint64_t timestamp_ms, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_pending_request_complete(UnitLabMmsPendingRequest* request, uint64_t completed_at_ms, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_pending_request_mark_timed_out(UnitLabMmsPendingRequest* request, uint64_t timed_out_at_ms, UnitLabMmsDiagnostic* diagnostic);
void unitlab_mms_associate_request_init(UnitLabMmsAssociateRequest* request);
void unitlab_mms_read_request_init(UnitLabMmsReadRequest* request);
void unitlab_mms_write_request_init(UnitLabMmsWriteRequest* request);
void unitlab_mms_information_report_init(UnitLabMmsInformationReport* report);
void unitlab_mms_operation_result_init(UnitLabMmsOperationResult* result);
void unitlab_mms_operation_result_from_event(UnitLabMmsOperationResult* result, int ok, const UnitLabMmsDiagnostic* diagnostic, const UnitLabMmsRuntimeEvent* event);
void unitlab_mms_operation_result_from_trace(UnitLabMmsOperationResult* result, int ok, const UnitLabMmsDiagnostic* diagnostic, const UnitLabMmsRuntimeEventLog* trace, const UnitLabMmsRuntimeEvent* event);
void unitlab_mms_runtime_snapshot_init(UnitLabMmsRuntimeSnapshot* snapshot);
void unitlab_mms_runtime_snapshot_capture(UnitLabMmsRuntimeSnapshot* snapshot, const UnitLabMmsSession* session, const UnitLabIec61850ReportControl* report_control, const UnitLabMmsTransportExchange* transport, const UnitLabMmsOperationResult* last_result);
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
