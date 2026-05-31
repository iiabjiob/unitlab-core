#include "unitlab_mms_semantic_pdu.h"

#include <string.h>

void unitlab_mms_decoded_pdu_init(UnitLabMmsDecodedPdu* pdu)
{
    if (pdu == NULL) {
        return;
    }
    memset(pdu, 0, sizeof(*pdu));
    pdu->kind = UNITLAB_MMS_DECODED_PDU_NONE;
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
    unitlab_mms_decoded_pdu_init(&result->pdu);
    unitlab_mms_decode_diagnostic_init(&result->diagnostic);
}

void unitlab_mms_decode_diagnostic_set(UnitLabMmsDecodeDiagnostic* diagnostic, UnitLabMmsDecodeClassification classification, UnitLabMmsDiagnosticCode code, const char* detail)
{
    if (diagnostic == NULL) {
        return;
    }
    diagnostic->classification = classification;
    diagnostic->diagnostic.code = code;
    if (detail == NULL) {
        diagnostic->detail[0] = '\0';
        diagnostic->diagnostic.message[0] = '\0';
        return;
    }
    strncpy(diagnostic->detail, detail, sizeof(diagnostic->detail) - 1U);
    diagnostic->detail[sizeof(diagnostic->detail) - 1U] = '\0';
    strncpy(diagnostic->diagnostic.message, detail, sizeof(diagnostic->diagnostic.message) - 1U);
    diagnostic->diagnostic.message[sizeof(diagnostic->diagnostic.message) - 1U] = '\0';
}
