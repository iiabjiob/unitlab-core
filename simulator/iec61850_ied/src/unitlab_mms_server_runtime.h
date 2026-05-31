#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_H

#include "unitlab_mms_core.h"
#include "wire/mms/unitlab_mms_pdu.h"

/*
 * Server-side ownership aggregate for the UnitLab-owned MMS lower layer.
 *
 * This is intentionally transport-agnostic: it owns the per-server/session
 * runtime objects and snapshots, but does not do socket, TPKT, or ACSE I/O.
 */

typedef struct UnitLabMmsServerRuntime {
    UnitLabMmsSession session;
    UnitLabMmsPendingRequest pending_request;
    UnitLabIec61850ReportControl report_control;
    UnitLabMmsTransportExchange transport;
    UnitLabMmsOperationResult last_result;
    UnitLabMmsRuntimeSnapshot snapshot;
} UnitLabMmsServerRuntime;

void unitlab_mms_server_runtime_init(UnitLabMmsServerRuntime* server_runtime);
int unitlab_mms_server_runtime_apply_wire_pdu(UnitLabMmsServerRuntime* server_runtime, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result);
void unitlab_mms_server_runtime_capture_snapshot(UnitLabMmsServerRuntime* server_runtime);

#endif /* UNITLAB_IEC61850_IED_UNITLAB_MMS_SERVER_RUNTIME_H */
