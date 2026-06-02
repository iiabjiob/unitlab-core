#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_TRANSPORT_FRAME_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_TRANSPORT_FRAME_H

#include "../../protocols/mms/unitlab_mms_types.h"
#include "../iso/unitlab_mms_cotp.h"
#include "../iso/unitlab_mms_tpkt.h"

/* One ISO-on-TCP frame: TPKT carrying exactly one COTP TPDU. */
typedef struct UnitLabMmsTransportFrame {
    UnitLabMmsTpktHeader tpkt;
    UnitLabMmsCotpTpdu cotp;
    size_t encoded_length;
} UnitLabMmsTransportFrame;

void unitlab_mms_transport_frame_init(UnitLabMmsTransportFrame* frame);
int unitlab_mms_transport_frame_encode(const UnitLabMmsTransportFrame* frame, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_transport_frame_decode(UnitLabMmsTransportFrame* frame, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
