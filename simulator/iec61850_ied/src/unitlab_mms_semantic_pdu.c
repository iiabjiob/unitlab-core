#include "unitlab_mms_semantic_pdu.h"

#include <string.h>

void unitlab_mms_decoded_pdu_init(UnitLabMmsDecodedPdu* pdu)
{
    if (pdu == NULL) {
        return;
    }
    memset(pdu, 0, sizeof(*pdu));
    pdu->kind = UNITLAB_MMS_DECODED_PDU_NONE;
    pdu->reject.reject_for_invoke_id = 0U;
}

void unitlab_mms_decode_diagnostic_init(UnitLabMmsDecodeDiagnostic* diagnostic)
{
    if (diagnostic == NULL) {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->classification = UNITLAB_MMS_DECODE_CLASSIFICATION_NONE;
    unitlab_mms_diagnostic_clear(&diagnostic->diagnostic);
}

void unitlab_mms_semantic_result_init(UnitLabMmsSemanticResult* result)
{
    if (result == NULL) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->outcome = UNITLAB_MMS_SERVICE_OUTCOME_NONE;
    unitlab_mms_decoded_pdu_init(&result->pdu);
    unitlab_mms_decode_diagnostic_init(&result->diagnostic);
}

void unitlab_mms_decode_diagnostic_set(UnitLabMmsDecodeDiagnostic* diagnostic, UnitLabMmsDecodeClassification classification, UnitLabMmsDiagnosticCode code, const char* detail, const char* message)
{
    if (diagnostic == NULL) {
        return;
    }
    diagnostic->classification = classification;
    diagnostic->diagnostic.code = code;
    if (detail == NULL) {
        diagnostic->detail[0] = '\0';
    } else {
        strncpy(diagnostic->detail, detail, sizeof(diagnostic->detail) - 1U);
        diagnostic->detail[sizeof(diagnostic->detail) - 1U] = '\0';
    }
    if (message == NULL) {
        message = detail;
    }
    if (message == NULL) {
        diagnostic->diagnostic.message[0] = '\0';
        return;
    }
    strncpy(diagnostic->diagnostic.message, message, sizeof(diagnostic->diagnostic.message) - 1U);
    diagnostic->diagnostic.message[sizeof(diagnostic->diagnostic.message) - 1U] = '\0';
}

void unitlab_mms_semantic_result_from_decoded_pdu(UnitLabMmsSemanticResult* result, UnitLabMmsServiceOutcome outcome, const UnitLabMmsDecodedPdu* pdu, const UnitLabMmsDecodeDiagnostic* diagnostic)
{
    if (result == NULL) {
        return;
    }
    unitlab_mms_semantic_result_init(result);
    if (diagnostic != NULL) {
        result->diagnostic = *diagnostic;
    }
    if (diagnostic == NULL || diagnostic->classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE) {
        result->ok = 1;
        result->outcome = outcome;
        if (pdu != NULL) {
            result->pdu = *pdu;
        }
        return;
    }
    result->ok = 0;
    result->outcome = UNITLAB_MMS_SERVICE_OUTCOME_NONE;
    unitlab_mms_decoded_pdu_init(&result->pdu);
}
