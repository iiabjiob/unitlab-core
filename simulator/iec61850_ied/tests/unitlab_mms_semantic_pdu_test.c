#include "unitlab_mms_semantic_pdu.h"

#include <assert.h>
#include <string.h>

static void test_defaults(void)
{
    UnitLabMmsDecodedPdu pdu;
    UnitLabMmsDecodeDiagnostic diagnostic;
    UnitLabMmsSemanticResult result;

    memset(&pdu, 0xA5, sizeof(pdu));
    memset(&diagnostic, 0xA5, sizeof(diagnostic));
    memset(&result, 0xA5, sizeof(result));

    unitlab_mms_decoded_pdu_init(&pdu);
    unitlab_mms_decode_diagnostic_init(&diagnostic);
    unitlab_mms_semantic_result_init(&result);

    assert(pdu.kind == UNITLAB_MMS_DECODED_PDU_NONE);
    assert(pdu.invoke_id == 0U);
    assert(pdu.value_bytes == NULL);
    assert(pdu.value_length == 0U);
    assert(diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
    assert(diagnostic.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(diagnostic.detail[0] == '\0');
    assert(result.ok == 0);
    assert(result.pdu.kind == UNITLAB_MMS_DECODED_PDU_NONE);
    assert(result.diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_NONE);
}

static void test_diagnostic_set(void)
{
    UnitLabMmsDecodeDiagnostic diagnostic;

    unitlab_mms_decode_diagnostic_init(&diagnostic);
    unitlab_mms_decode_diagnostic_set(&diagnostic, UNITLAB_MMS_DECODE_CLASSIFICATION_DECODE_FAILURE, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "malformed input");

    assert(diagnostic.classification == UNITLAB_MMS_DECODE_CLASSIFICATION_DECODE_FAILURE);
    assert(diagnostic.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR);
    assert(strcmp(diagnostic.detail, "malformed input") == 0);
    assert(strcmp(diagnostic.diagnostic.message, "malformed input") == 0);
}

int main(void)
{
    test_defaults();
    test_diagnostic_set();
    return 0;
}
