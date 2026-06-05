#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_CORE_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_CORE_H

#include "protocols/mms/unitlab_mms_types.h"
#include "protocols/mms/unitlab_mms_semantic_pdu.h"
#include "report/unitlab_iec61850_report_runtime.h"

typedef struct UnitLabIedModelPlan UnitLabIedModelPlan;

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
    UNITLAB_MMS_REQUEST_GI = 9,
    UNITLAB_MMS_REQUEST_GET_NAME_LIST = 10,
    UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES = 11,
    UNITLAB_MMS_REQUEST_GET_NAMED_VARIABLE_LIST_ATTRIBUTES = 12
} UnitLabMmsRequestKind;

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
    char object_reference[128];
    char attribute_reference[64];
    uint32_t browse_object_class;
    uint32_t browse_object_scope;
    char browse_domain_id[128];
    char browse_continue_after[128];
    UnitLabMmsRuntimeEvent last_event;
    UnitLabMmsRuntimeEventLog event_log;
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

typedef struct UnitLabMmsSession {
    UnitLabMmsSessionState state;
    uint32_t next_invoke_id;
    uint32_t active_invoke_id;
    UnitLabMmsRuntimeEvent last_event;
    UnitLabMmsRuntimeEventLog event_log;
} UnitLabMmsSession;

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
    UnitLabMmsTransportExchange transport;
    UnitLabMmsOperationResult last_result;
    UnitLabIec61850ReportControlState report_control_state;
    UnitLabMmsRuntimeEvent report_control_last_event;
    UnitLabMmsRuntimeEventLog report_control_event_log;
} UnitLabMmsRuntimeSnapshot;

void unitlab_mms_runtime_snapshot_init(UnitLabMmsRuntimeSnapshot* snapshot);
void unitlab_mms_runtime_snapshot_capture(UnitLabMmsRuntimeSnapshot* snapshot, const UnitLabMmsSession* session, const UnitLabIec61850ReportControl* report_control, const UnitLabMmsTransportExchange* transport, const UnitLabMmsOperationResult* last_result);
void unitlab_mms_session_init(UnitLabMmsSession* session);
void unitlab_mms_session_reset(UnitLabMmsSession* session);
uint32_t unitlab_mms_session_next_invoke_id(UnitLabMmsSession* session);
int unitlab_mms_session_begin_association(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_session_complete_association(UnitLabMmsSession* session, uint32_t invoke_id, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_session_begin_release(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_session_complete_release(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_runtime_apply_semantic_result(UnitLabMmsSession* session, UnitLabMmsPendingRequest* pending_request, const UnitLabMmsSemanticResult* semantic_result, UnitLabMmsOperationResult* operation_result);
int unitlab_mms_session_abort(UnitLabMmsSession* session, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_session_is_associated(const UnitLabMmsSession* session);
void unitlab_mms_pending_request_init(UnitLabMmsPendingRequest* request);
int unitlab_mms_pending_request_start(UnitLabMmsPendingRequest* request, UnitLabMmsRequestKind kind, uint32_t invoke_id, uint32_t correlation_id, uint64_t deadline_ms, uint64_t timestamp_ms, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_pending_request_complete(UnitLabMmsPendingRequest* request, uint64_t completed_at_ms, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_pending_request_mark_timed_out(UnitLabMmsPendingRequest* request, uint64_t timed_out_at_ms, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_pending_request_collect_get_name_list_names(const UnitLabMmsPendingRequest* request, const UnitLabIedModelPlan* plan, char*** names, size_t* count, UnitLabMmsDiagnostic* diagnostic);
void unitlab_mms_associate_request_init(UnitLabMmsAssociateRequest* request);
void unitlab_mms_read_request_init(UnitLabMmsReadRequest* request);
void unitlab_mms_write_request_init(UnitLabMmsWriteRequest* request);
void unitlab_mms_information_report_init(UnitLabMmsInformationReport* report);
void unitlab_mms_transport_exchange_init(UnitLabMmsTransportExchange* exchange);
int unitlab_mms_transport_exchange_bind_request(UnitLabMmsTransportExchange* exchange, const uint8_t* request_bytes, size_t request_length, uint32_t invoke_id, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_transport_exchange_bind_response(UnitLabMmsTransportExchange* exchange, uint8_t* response_bytes, size_t response_capacity, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_transport_exchange_set_response_length(UnitLabMmsTransportExchange* exchange, size_t response_length, UnitLabMmsDiagnostic* diagnostic);

#endif
