#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_ASSOCIATION_FRAME_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_ASSOCIATION_FRAME_H

#include "../../unitlab_mms_types.h"
#include "../presentation/unitlab_mms_presentation.h"
#include "../session/unitlab_mms_session_spdu.h"
#include "../transport/unitlab_mms_transport_frame.h"

/*
 * Production helper for one ISO-on-TCP association-style nested payload.
 * The encoder owns the full nesting construction: the caller provides raw
 * Presentation payload bytes, and the helper composes TPKT + COTP + Session SPDU
 * + exact X.226 Presentation User-data. transport.cotp.user_data is not an input
 * to the encode contract.
 */
typedef struct UnitLabMmsAssociationFrame {
    UnitLabMmsTransportFrame transport;
    UnitLabMmsSessionSpdu session;
    UnitLabMmsPresentationApdu presentation;
    size_t encoded_length;
} UnitLabMmsAssociationFrame;

void unitlab_mms_association_frame_init(UnitLabMmsAssociationFrame* frame);
int unitlab_mms_association_frame_encode(const UnitLabMmsAssociationFrame* frame, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_association_frame_decode(UnitLabMmsAssociationFrame* frame, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
