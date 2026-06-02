#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_COTP_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_COTP_H

#include "../../protocols/mms/unitlab_mms_types.h"

typedef enum UnitLabMmsCotpTpduKind {
    UNITLAB_MMS_COTP_TPDU_NONE = 0,
    UNITLAB_MMS_COTP_TPDU_CR = 1,
    UNITLAB_MMS_COTP_TPDU_CC = 2,
    UNITLAB_MMS_COTP_TPDU_DR = 3,
    UNITLAB_MMS_COTP_TPDU_DT = 4
} UnitLabMmsCotpTpduKind;

/* X.224 TPDU codes are classified exactly; the TPDU payload remains opaque raw bytes for replay/debug. */

typedef struct UnitLabMmsCotpTpdu {
    UnitLabMmsCotpTpduKind kind;
    uint16_t destination_reference;
    uint16_t source_reference;
    uint8_t tpdu_class;
    uint8_t reason;
    int eot;
    const uint8_t* payload_bytes; /* Caller-owned decode buffer; valid only while that buffer lives. */
    size_t payload_length;
    const uint8_t* user_data;
    size_t user_data_length;
    size_t encoded_length;
} UnitLabMmsCotpTpdu;

void unitlab_mms_cotp_tpdu_init(UnitLabMmsCotpTpdu* tpdu);
int unitlab_mms_cotp_encode(const UnitLabMmsCotpTpdu* tpdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_cotp_decode(UnitLabMmsCotpTpdu* tpdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
