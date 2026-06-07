#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_H

#include "server/server_runtime.h"
#include "protocols/mms/unitlab_mms_core.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"

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

#define UNITLAB_MMS_SERVER_RUNTIME_WIRE_SCRATCH_LENGTH 4096U

typedef struct UnitLabMmsServerRuntime {
    UnitLabMmsServerRuntimeState state;
    UnitLabIedServerConfig config;
    UnitLabMmsSession session;
    UnitLabMmsPendingRequest pending_request;
    const UnitLabIedModelPlan* model_plan;
    UnitLabMmsInitiateResponseProfile initiate_response_profile;
    char read_response_value[128];
    size_t read_response_value_length;
    int has_read_response_value;
    uint32_t last_incoming_invoke_id;
    char last_incoming_service[64];
    uint32_t last_outgoing_invoke_id;
    char last_outgoing_service[64];
    char last_outgoing_summary[512];
    UnitLabIec61850ReportControl report_control;
    uint8_t brcb_rpt_ena;
    uint32_t brcb_resv_tms;
    uint32_t brcb_sq_num;
    uint64_t brcb_entry_id_counter;
    uint8_t brcb_entry_id[8];
    uint8_t brcb_time_of_entry[6];
    uint8_t pending_gi_report;
    UnitLabMmsTransportExchange transport;
    UnitLabMmsOperationResult last_result;
    UnitLabMmsRuntimeSnapshot snapshot;
    UnitLabMmsPdu last_wire_pdu;
    uint8_t wire_scratch[UNITLAB_MMS_SERVER_RUNTIME_WIRE_SCRATCH_LENGTH];
} UnitLabMmsServerRuntime;

void unitlab_mms_server_runtime_init(UnitLabMmsServerRuntime* server_runtime);
int unitlab_mms_server_runtime_prepare(UnitLabMmsServerRuntime* server_runtime, const UnitLabIedServerConfig* config, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_start(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_apply_model_plan(UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelPlan* plan);
int unitlab_mms_server_runtime_stop(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_reserve_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_enable_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_request_general_interrogation(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_disable_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_release_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_apply_incoming_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result);
int unitlab_mms_server_runtime_apply_association_request_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result);
int unitlab_mms_server_runtime_apply_association_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result);
int unitlab_mms_server_runtime_build_confirmed_error_bytes(UnitLabMmsServerRuntime* server_runtime, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_build_confirmed_response_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* service_bytes, size_t service_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_build_release_response_bytes(UnitLabMmsServerRuntime* server_runtime, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_has_pending_gi_report(const UnitLabMmsServerRuntime* server_runtime);
int unitlab_mms_server_runtime_build_pending_gi_report_bytes(UnitLabMmsServerRuntime* server_runtime, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_server_runtime_apply_wire_pdu(UnitLabMmsServerRuntime* server_runtime, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result);
void unitlab_mms_server_runtime_capture_snapshot(UnitLabMmsServerRuntime* server_runtime);

#endif /* UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_H */
