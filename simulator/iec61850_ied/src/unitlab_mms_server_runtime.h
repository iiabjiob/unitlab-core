#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_H

#include "server_runtime.h"
#include "unitlab_mms_core.h"
#include "wire/mms/unitlab_mms_pdu.h"

/*
 * Server-side ownership aggregate for the UnitLab-owned MMS lower layer.
 *
 * This is intentionally transport-agnostic: it owns the per-server/session
 * runtime objects and snapshots, but does not do socket, TPKT, or ACSE I/O.
 */

typedef enum UnitLabMmsServerRuntimeState {
    UNITLAB_MMS_SERVER_RUNTIME_IDLE = 0,
    UNITLAB_MMS_SERVER_RUNTIME_PREPARED = 1,
    UNITLAB_MMS_SERVER_RUNTIME_RUNNING = 2,
    UNITLAB_MMS_SERVER_RUNTIME_STOPPED = 3,
    UNITLAB_MMS_SERVER_RUNTIME_FAILED = 4
} UnitLabMmsServerRuntimeState;

typedef struct UnitLabMmsServerRuntime {
    UnitLabMmsServerRuntimeState state;
    UnitLabIedServerConfig config;
    UnitLabMmsSession session;
    UnitLabMmsPendingRequest pending_request;
    UnitLabIec61850ReportControl report_control;
    UnitLabMmsTransportExchange transport;
    UnitLabMmsOperationResult last_result;
    UnitLabMmsRuntimeSnapshot snapshot;
} UnitLabMmsServerRuntime;

void unitlab_mms_server_runtime_init(UnitLabMmsServerRuntime* server_runtime);
int unitlab_mms_server_runtime_prepare(UnitLabMmsServerRuntime* server_runtime, const UnitLabIedServerConfig* config, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_start(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_stop(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_reserve_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_enable_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_request_general_interrogation(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_disable_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_release_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_apply_incoming_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result);
int unitlab_mms_server_runtime_apply_association_request_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result);
int unitlab_mms_server_runtime_apply_association_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result);
int unitlab_mms_server_runtime_build_confirmed_response_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* service_bytes, size_t service_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_apply_wire_pdu(UnitLabMmsServerRuntime* server_runtime, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result);
void unitlab_mms_server_runtime_capture_snapshot(UnitLabMmsServerRuntime* server_runtime);

#endif /* UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_H */
