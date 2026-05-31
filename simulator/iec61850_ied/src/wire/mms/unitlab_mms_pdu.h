#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_PDU_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_PDU_H

#include "../../unitlab_mms_types.h"
#include "../ber/unitlab_mms_ber.h"

typedef enum UnitLabMmsPduKind {
    UNITLAB_MMS_PDU_NONE = 0,
    UNITLAB_MMS_PDU_CONFIRMED_REQUEST = 1,
    UNITLAB_MMS_PDU_CONFIRMED_RESPONSE = 2,
    UNITLAB_MMS_PDU_CONFIRMED_ERROR = 3,
    UNITLAB_MMS_PDU_UNCONFIRMED = 4,
    UNITLAB_MMS_PDU_REJECT = 5,
    UNITLAB_MMS_PDU_INITIATE_REQUEST = 6,
    UNITLAB_MMS_PDU_INITIATE_RESPONSE = 7,
    UNITLAB_MMS_PDU_INITIATE_ERROR = 8,
    UNITLAB_MMS_PDU_CONCLUDE_REQUEST = 9,
    UNITLAB_MMS_PDU_CONCLUDE_RESPONSE = 10,
    UNITLAB_MMS_PDU_CONCLUDE_ERROR = 11
} UnitLabMmsPduKind;

typedef struct UnitLabMmsPdu {
    UnitLabMmsPduKind kind;
    uint32_t invoke_id;
    int has_invoke_id;
    UnitLabMmsBerTag service_tag;
    int has_service;
    const uint8_t* service_bytes; /* Caller-owned decode buffer; valid only while that buffer lives. */
    size_t service_length;
    const uint8_t* pdu_bytes; /* Caller-owned decode buffer; valid only while that buffer lives. */
    size_t pdu_length;
    size_t encoded_length;
} UnitLabMmsPdu;

void unitlab_mms_pdu_init(UnitLabMmsPdu* pdu);
int unitlab_mms_pdu_encode(const UnitLabMmsPdu* pdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_pdu_decode(UnitLabMmsPdu* pdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
