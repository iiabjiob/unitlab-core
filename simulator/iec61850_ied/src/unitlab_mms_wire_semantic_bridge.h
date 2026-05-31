#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_SEMANTIC_BRIDGE_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_SEMANTIC_BRIDGE_H

#include "unitlab_mms_semantic_pdu.h"
#include "wire/mms/unitlab_mms_pdu.h"

/*
 * Bridge from wire-level MMS PDU classification to the transport-independent semantic result.
 * This handles only the first-slice services and does not interpret runtime state.
 */
int unitlab_mms_semantic_result_from_wire_pdu(UnitLabMmsSemanticResult* result, const UnitLabMmsPdu* wire_pdu, UnitLabMmsDecodeDiagnostic* diagnostic);

#endif
