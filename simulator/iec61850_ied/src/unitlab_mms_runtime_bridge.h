#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_RUNTIME_BRIDGE_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_RUNTIME_BRIDGE_H

#include "unitlab_mms_core.h"
#include "unitlab_mms_wire_semantic_bridge.h"

/*
 * Applies a wire PDU through the semantic bridge and then the runtime ownership layer.
 * Wire decode remains state-neutral; lifecycle changes are owned by the runtime.
 */
int unitlab_mms_runtime_apply_wire_pdu(UnitLabMmsSession* session, UnitLabMmsPendingRequest* pending_request, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result);
int unitlab_mms_runtime_apply_wire_pdu_with_report_control(UnitLabMmsSession* session, UnitLabMmsPendingRequest* pending_request, UnitLabIec61850ReportControl* report_control, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result);

#endif
