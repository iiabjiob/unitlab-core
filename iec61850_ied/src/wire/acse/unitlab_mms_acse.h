#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_ACSE_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_ACSE_H

#include "../../protocols/mms/unitlab_mms_types.h"
#include "../ber/unitlab_mms_ber.h"

typedef enum UnitLabMmsAcseApduKind {
    UNITLAB_MMS_ACSE_APDU_NONE = 0,
    UNITLAB_MMS_ACSE_APDU_AARQ = 1,
    UNITLAB_MMS_ACSE_APDU_AARE = 2,
    UNITLAB_MMS_ACSE_APDU_RLRQ = 3,
    UNITLAB_MMS_ACSE_APDU_RLRE = 4,
    UNITLAB_MMS_ACSE_APDU_ABRT = 5
} UnitLabMmsAcseApduKind;

/* X.227 ACSE top-level APDUs are parsed as exact raw field sequences; no semantic interpretation happens here. */

typedef struct UnitLabMmsAcseApdu {
    UnitLabMmsAcseApduKind kind;
    const uint8_t* apdu_bytes; /* Caller-owned decode buffer; valid only while that buffer lives. */
    size_t apdu_length;
    UnitLabMmsBerElement fields[16]; /* Raw decoded field view; pointers reference the caller-owned decode buffer. */
    size_t field_count;
    size_t encoded_length;
} UnitLabMmsAcseApdu;

void unitlab_mms_acse_apdu_init(UnitLabMmsAcseApdu* apdu);
int unitlab_mms_acse_encode(const UnitLabMmsAcseApdu* apdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_acse_build_association_accept_frame(const uint8_t* initiate_response_bytes, size_t initiate_response_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_acse_decode(UnitLabMmsAcseApdu* apdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
