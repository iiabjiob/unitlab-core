#include "unitlab_mms_runtime_bridge.h"

int unitlab_mms_runtime_apply_wire_pdu(UnitLabMmsSession* session, UnitLabMmsPendingRequest* pending_request, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result)
{
    UnitLabMmsSemanticResult semantic_result;
    UnitLabMmsDecodeDiagnostic bridge_diagnostic;

    if (operation_result == NULL) {
        return 0;
    }
    unitlab_mms_operation_result_init(operation_result);
    unitlab_mms_semantic_result_init(&semantic_result);
    unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);

    if (!unitlab_mms_semantic_result_from_wire_pdu(&semantic_result, wire_pdu, &bridge_diagnostic)) {
        operation_result->diagnostic = bridge_diagnostic.diagnostic;
        operation_result->ok = 0;
        return 0;
    }

    if (!unitlab_mms_runtime_apply_semantic_result(session, pending_request, &semantic_result, operation_result)) {
        return 0;
    }
    return 1;
}
