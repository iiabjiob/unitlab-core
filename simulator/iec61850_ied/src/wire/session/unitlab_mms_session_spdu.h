#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_SESSION_SPDU_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_SESSION_SPDU_H

#include "../../unitlab_mms_types.h"

/*
 * Raw full-buffer session-SPDU boundary.
 * Exact X.225 session kernel SPDU codes are classified here, but the SPDU body stays opaque.
 * This wrapper consumes the whole supplied buffer and is not yet a general X.225 SPDU stream parser.
 * No runtime state transition is performed by this layer.
 */
typedef enum UnitLabMmsSessionSpduKind {
    UNITLAB_MMS_SESSION_SPDU_NONE = 0,
    UNITLAB_MMS_SESSION_SPDU_CONNECT = 1,
    UNITLAB_MMS_SESSION_SPDU_CONNECT_DATA_OVERFLOW = 2,
    UNITLAB_MMS_SESSION_SPDU_OVERFLOW_ACCEPT = 3,
    UNITLAB_MMS_SESSION_SPDU_ACCEPT = 4,
    UNITLAB_MMS_SESSION_SPDU_REFUSE = 5,
    UNITLAB_MMS_SESSION_SPDU_FINISH = 6,
    UNITLAB_MMS_SESSION_SPDU_DISCONNECT = 7,
    UNITLAB_MMS_SESSION_SPDU_NOT_FINISHED = 8,
    UNITLAB_MMS_SESSION_SPDU_ABORT = 9,
    UNITLAB_MMS_SESSION_SPDU_ABORT_ACCEPT = 10,
    UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER = 11,
    UNITLAB_MMS_SESSION_SPDU_EXPEDITED_DATA = 12,
    UNITLAB_MMS_SESSION_SPDU_TYPED_DATA = 13,
    UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA = 14,
    UNITLAB_MMS_SESSION_SPDU_CAPABILITY_DATA_ACK = 15
} UnitLabMmsSessionSpduKind;

typedef struct UnitLabMmsSessionSpdu {
    UnitLabMmsSessionSpduKind kind;
    const uint8_t* spdu_bytes; /* Caller-owned decode buffer; valid only while that buffer lives. */
    size_t spdu_length;
    const uint8_t* raw_parameter_bytes; /* Raw parameter view inside spdu_bytes; caller-owned decode buffer. */
    size_t raw_parameter_length;
    size_t encoded_length;
} UnitLabMmsSessionSpdu;

void unitlab_mms_session_spdu_init(UnitLabMmsSessionSpdu* spdu);
int unitlab_mms_session_spdu_encode(const UnitLabMmsSessionSpdu* spdu, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_session_spdu_decode(UnitLabMmsSessionSpdu* spdu, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
