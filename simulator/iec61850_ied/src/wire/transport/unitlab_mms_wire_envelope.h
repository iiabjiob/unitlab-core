#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_ENVELOPE_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_ENVELOPE_H

#include "../../unitlab_mms_types.h"
#include "../acse/unitlab_mms_acse.h"
#include "../mms/unitlab_mms_pdu.h"
#include "../presentation/unitlab_mms_presentation.h"
#include "unitlab_mms_transport_frame.h"

typedef struct UnitLabMmsWireEnvelope {
    UnitLabMmsTransportFrame transport;
    UnitLabMmsPresentationApdu presentation;
    UnitLabMmsAcseApdu acse;
    UnitLabMmsPdu pdu;
    size_t encoded_length;
} UnitLabMmsWireEnvelope;

void unitlab_mms_wire_envelope_init(UnitLabMmsWireEnvelope* envelope);
int unitlab_mms_wire_envelope_decode(UnitLabMmsWireEnvelope* envelope, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
