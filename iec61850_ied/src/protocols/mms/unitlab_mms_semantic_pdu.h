#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_SEMANTIC_PDU_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_SEMANTIC_PDU_H

#include "protocols/mms/unitlab_mms_types.h"

typedef enum UnitLabMmsDecodeClassification {
    UNITLAB_MMS_DECODE_CLASSIFICATION_NONE = 0,
    UNITLAB_MMS_DECODE_CLASSIFICATION_DECODE_FAILURE = 1,
    UNITLAB_MMS_DECODE_CLASSIFICATION_SEMANTIC_INVALID = 2,
    UNITLAB_MMS_DECODE_CLASSIFICATION_UNSUPPORTED_SEMANTIC = 3,
    UNITLAB_MMS_DECODE_CLASSIFICATION_UNEXPECTED_SERVICE = 4,
    UNITLAB_MMS_DECODE_CLASSIFICATION_CORRELATION_MISMATCH = 5
} UnitLabMmsDecodeClassification;

typedef enum UnitLabMmsServiceOutcome {
    UNITLAB_MMS_SERVICE_OUTCOME_NONE = 0,
    UNITLAB_MMS_SERVICE_OUTCOME_SUCCESS = 1,
    UNITLAB_MMS_SERVICE_OUTCOME_REJECT = 2,
    UNITLAB_MMS_SERVICE_OUTCOME_ERROR = 3
} UnitLabMmsServiceOutcome;

typedef enum UnitLabMmsDecodedPduKind {
    UNITLAB_MMS_DECODED_PDU_NONE = 0,
    UNITLAB_MMS_DECODED_PDU_ASSOCIATE_REQUEST = 1,
    UNITLAB_MMS_DECODED_PDU_ASSOCIATE_RESPONSE = 2,
    UNITLAB_MMS_DECODED_PDU_RELEASE_REQUEST = 3,
    UNITLAB_MMS_DECODED_PDU_RELEASE_RESPONSE = 4,
    UNITLAB_MMS_DECODED_PDU_READ_REQUEST = 5,
    UNITLAB_MMS_DECODED_PDU_READ_RESPONSE = 6,
    UNITLAB_MMS_DECODED_PDU_WRITE_REQUEST = 7,
    UNITLAB_MMS_DECODED_PDU_WRITE_RESPONSE = 8,
    UNITLAB_MMS_DECODED_PDU_INFORMATION_REPORT = 9,
    UNITLAB_MMS_DECODED_PDU_ABORT = 10,
    UNITLAB_MMS_DECODED_PDU_REJECT = 11,
    UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_REQUEST = 12,
    UNITLAB_MMS_DECODED_PDU_GET_NAME_LIST_RESPONSE = 13,
    UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_REQUEST = 14,
    UNITLAB_MMS_DECODED_PDU_GET_VARIABLE_ACCESS_ATTRIBUTES_RESPONSE = 15,
    UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_REQUEST = 16,
    UNITLAB_MMS_DECODED_PDU_GET_NAMED_VARIABLE_LIST_ATTRIBUTES_RESPONSE = 17
} UnitLabMmsDecodedPduKind;

typedef struct UnitLabMmsDecodedPdu {
    UnitLabMmsDecodedPduKind kind;
    uint32_t invoke_id;
    uint32_t correlation_id;
    uint64_t timestamp_ms;
    uint64_t deadline_ms;
    char object_reference[128];
    char attribute_reference[64];
    char report_control_reference[128];
    char data_set_reference[128];
    uint32_t object_class;
    uint32_t object_scope;
    char domain_id[128];
    char continue_after[128];
    char item_id[128];
    const uint8_t* value_bytes; /* Caller-owned decode buffer; valid only while the buffer lives. */
    size_t value_length;
    int buffered;
    UnitLabMmsReject reject;
} UnitLabMmsDecodedPdu;

typedef struct UnitLabMmsDecodeDiagnostic {
    UnitLabMmsDecodeClassification classification;
    UnitLabMmsDiagnostic diagnostic;
    char detail[256]; /* Technical decode context; diagnostic.message remains the human-readable message. */
} UnitLabMmsDecodeDiagnostic;

typedef struct UnitLabMmsSemanticResult {
    int ok;
    UnitLabMmsServiceOutcome outcome;
    UnitLabMmsDecodedPdu pdu;
    UnitLabMmsDecodeDiagnostic diagnostic;
} UnitLabMmsSemanticResult;

void unitlab_mms_decoded_pdu_init(UnitLabMmsDecodedPdu* pdu);
void unitlab_mms_decode_diagnostic_init(UnitLabMmsDecodeDiagnostic* diagnostic);
void unitlab_mms_semantic_result_init(UnitLabMmsSemanticResult* result);
void unitlab_mms_decode_diagnostic_set(UnitLabMmsDecodeDiagnostic* diagnostic, UnitLabMmsDecodeClassification classification, UnitLabMmsDiagnosticCode code, const char* detail, const char* message);
void unitlab_mms_semantic_result_from_decoded_pdu(UnitLabMmsSemanticResult* result, UnitLabMmsServiceOutcome outcome, const UnitLabMmsDecodedPdu* pdu, const UnitLabMmsDecodeDiagnostic* diagnostic);

#endif
