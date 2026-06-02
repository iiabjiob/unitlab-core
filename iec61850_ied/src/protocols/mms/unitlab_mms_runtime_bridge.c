#include "protocols/mms/unitlab_mms_runtime_bridge.h"

int unitlab_mms_runtime_apply_wire_pdu_with_report_control(UnitLabMmsSession* session, UnitLabMmsPendingRequest* pending_request, UnitLabIec61850ReportControl* report_control, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result)
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

    if (semantic_result.ok && semantic_result.pdu.kind == UNITLAB_MMS_DECODED_PDU_INFORMATION_REPORT && report_control != NULL) {
        UnitLabMmsDiagnostic report_diagnostic;
        unitlab_mms_diagnostic_clear(&report_diagnostic);
        if (!unitlab_iec61850_report_control_accept_report(report_control, semantic_result.pdu.invoke_id, &report_diagnostic)) {
            unitlab_mms_operation_result_from_trace(operation_result, 0, &report_diagnostic, &report_control->event_log, &report_control->last_event);
            return 0;
        }
        unitlab_mms_operation_result_from_trace(operation_result, 1, &report_diagnostic, &report_control->event_log, &report_control->last_event);
        return 1;
    }

    if (!unitlab_mms_runtime_apply_semantic_result(session, pending_request, &semantic_result, operation_result)) {
        return 0;
    }
    return 1;
}

int unitlab_mms_runtime_apply_wire_pdu(UnitLabMmsSession* session, UnitLabMmsPendingRequest* pending_request, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result)
{
    return unitlab_mms_runtime_apply_wire_pdu_with_report_control(session, pending_request, NULL, wire_pdu, operation_result);
}
