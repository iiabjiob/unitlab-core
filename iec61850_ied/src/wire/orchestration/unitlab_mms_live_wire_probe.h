#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_LIVE_WIRE_PROBE_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_LIVE_WIRE_PROBE_H

#include <stddef.h>
#include <stdint.h>

#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"

int unitlab_mms_build_live_wire_association_request_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

#endif /* UNITLAB_IEC61850_IED_UNITLAB_MMS_LIVE_WIRE_PROBE_H */
