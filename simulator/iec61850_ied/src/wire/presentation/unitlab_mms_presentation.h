#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_PRESENTATION_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_PRESENTATION_H

#include "../../unitlab_mms_types.h"
#include "../ber/unitlab_mms_ber.h"

typedef enum UnitLabMmsPresentationApduKind {
    UNITLAB_MMS_PRESENTATION_APDU_NONE = 0,
    UNITLAB_MMS_PRESENTATION_APDU_CONNECT_OR_ACCEPT = 1,
    UNITLAB_MMS_PRESENTATION_APDU_USER_DATA = 2,
    UNITLAB_MMS_PRESENTATION_APDU_ABORT = 3
} UnitLabMmsPresentationApduKind;

typedef struct UnitLabMmsPresentationApdu {
    UnitLabMmsPresentationApduKind kind;
    UnitLabMmsBerTag tag;
    const uint8_t* payload_bytes; /* Caller-owned decode buffer; valid only while that buffer lives. */
    size_t payload_length;
    size_t encoded_length;
} UnitLabMmsPresentationApdu;

void unitlab_mms_presentation_apdu_init(UnitLabMmsPresentationApdu* apdu);
int unitlab_mms_presentation_encode(const UnitLabMmsPresentationApdu* apdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_presentation_decode(UnitLabMmsPresentationApdu* apdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
