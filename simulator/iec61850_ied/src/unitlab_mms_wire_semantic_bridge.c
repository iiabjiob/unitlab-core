#include "unitlab_mms_wire_semantic_bridge.h"

static void bridge_set_diagnostic(UnitLabMmsDecodeDiagnostic* diagnostic, UnitLabMmsDecodeClassification classification, UnitLabMmsDiagnosticCode code, const char* detail, const char* message)
{
    unitlab_mms_decode_diagnostic_set(diagnostic, classification, code, detail, message);
}

static int bridge_map_pdu_kind(const UnitLabMmsPdu* wire_pdu, UnitLabMmsDecodedPdu* decoded_pdu, UnitLabMmsServiceOutcome* outcome, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    if (wire_pdu == NULL || decoded_pdu == NULL || outcome == NULL) {
        bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_DECODE_FAILURE, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "wire MMS PDU bridge requires wire_pdu, decoded_pdu, and outcome.", "wire MMS PDU bridge requires wire_pdu, decoded_pdu, and outcome.");
        return 0;
    }

    unitlab_mms_decoded_pdu_init(decoded_pdu);
    decoded_pdu->invoke_id = wire_pdu->invoke_id;
    decoded_pdu->value_bytes = wire_pdu->service_bytes;
    decoded_pdu->value_length = wire_pdu->service_length;

    switch (wire_pdu->kind) {
        case UNITLAB_MMS_PDU_INITIATE_REQUEST:
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_ASSOCIATE_REQUEST;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
            return 1;
        case UNITLAB_MMS_PDU_INITIATE_RESPONSE:
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_ASSOCIATE_RESPONSE;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
            return 1;
        case UNITLAB_MMS_PDU_CONFIRMED_REQUEST:
            if (!wire_pdu->has_invoke_id || !wire_pdu->has_service) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "confirmed MMS request is missing invokeID or service choice.", "confirmed MMS request is missing invokeID or service choice.");
                return 0;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_READ) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_READ_REQUEST;
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_WRITE) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_WRITE_REQUEST;
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "confirmed MMS request service is unsupported.", "confirmed MMS request service is unsupported.");
            return 0;
        case UNITLAB_MMS_PDU_CONFIRMED_RESPONSE:
            if (!wire_pdu->has_invoke_id || !wire_pdu->has_service) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "confirmed MMS response is missing invokeID or service choice.", "confirmed MMS response is missing invokeID or service choice.");
                return 0;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_READ) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_READ_RESPONSE;
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            if (wire_pdu->service_kind == UNITLAB_MMS_SERVICE_WRITE) {
                decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_WRITE_RESPONSE;
                *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
                return 1;
            }
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "confirmed MMS response service is unsupported.", "confirmed MMS response service is unsupported.");
            return 0;
        case UNITLAB_MMS_PDU_UNCONFIRMED:
            if (!wire_pdu->has_service || wire_pdu->service_kind != UNITLAB_MMS_SERVICE_INFORMATION_REPORT) {
                bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unconfirmed MMS service is unsupported.", "unconfirmed MMS service is unsupported.");
                return 0;
            }
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_INFORMATION_REPORT;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS;
            return 1;
        case UNITLAB_MMS_PDU_REJECT:
            decoded_pdu->kind = UNITLAB_MMS_DECODED_PDU_REJECT;
            *outcome = UNITLAB_MMS_SERVICE_OUTCOME_REJECT;
            return 1;
        default:
            bridge_set_diagnostic(diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "unsupported MMS PDU kind for semantic bridge.", "unsupported MMS PDU kind for semantic bridge.");
            return 0;
    }
}

int unitlab_mms_semantic_result_from_wire_pdu(UnitLabMmsSemanticResult* result, const UnitLabMmsPdu* wire_pdu, UnitLabMmsDecodeDiagnostic* diagnostic)
{
    UnitLabMmsDecodedPdu decoded_pdu;
    UnitLabMmsServiceOutcome outcome;
    UnitLabMmsDecodeDiagnostic bridge_diagnostic;

    if (result == NULL) {
        return 0;
    }
    unitlab_mms_semantic_result_init(result);
    unitlab_mms_decode_diagnostic_init(&bridge_diagnostic);
    unitlab_mms_decoded_pdu_init(&decoded_pdu);
    outcome = UNITLAB_MMS_SERVICE_OUTCOME_NONE;

    if (!bridge_map_pdu_kind(wire_pdu, &decoded_pdu, &outcome, &bridge_diagnostic)) {
        if (diagnostic != NULL) {
            *diagnostic = bridge_diagnostic;
        }
        result->diagnostic = bridge_diagnostic;
        return 0;
    }

    unitlab_mms_semantic_result_from_decoded_pdu(result, outcome, &decoded_pdu, &bridge_diagnostic);
    if (diagnostic != NULL) {
        *diagnostic = bridge_diagnostic;
    }
    return result->ok;
}

