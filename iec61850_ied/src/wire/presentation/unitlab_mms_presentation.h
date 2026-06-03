#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_PRESENTATION_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_PRESENTATION_H

#include "../../protocols/mms/unitlab_mms_types.h"
#include "../ber/unitlab_mms_ber.h"

typedef enum UnitLabMmsPresentationApduKind {
    UNITLAB_MMS_PRESENTATION_APDU_NONE = 0,
    UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED = 1,
    UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED = 2
} UnitLabMmsPresentationApduKind;

typedef struct UnitLabMmsPresentationApdu {
    UnitLabMmsPresentationApduKind kind;
    UnitLabMmsBerTag tag; /* Decoded outer Presentation BER element tag. */
    const uint8_t* payload_bytes; /* Caller-owned decode buffer; valid only while that buffer lives. */
    size_t payload_length;
    size_t encoded_length;
} UnitLabMmsPresentationApdu;

/*
 * Exact X.226 Presentation User-data boundary.
 * simply-encoded-data maps to [APPLICATION 0] IMPLICIT OCTET STRING.
 * fully-encoded-data maps to [APPLICATION 1] IMPLICIT SEQUENCE OF PDV-list,
 * where each PDV-list carries a presentation-context-identifier and a
 * single-ASN1-type wrapper around the ACSE/MMS payload.
 * Do not guess additional Presentation PDU mappings here.
 * No runtime state transition is performed by this layer.
 */
void unitlab_mms_presentation_apdu_init(UnitLabMmsPresentationApdu* apdu);
int unitlab_mms_presentation_encode(const UnitLabMmsPresentationApdu* apdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_presentation_decode(UnitLabMmsPresentationApdu* apdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
