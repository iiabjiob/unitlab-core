#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_BUILDER_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_BUILDER_H

#include "wire/mms/unitlab_mms_pdu.h"

/* Builds a wire frame by owning the nesting construction.
 * The caller provides a semantic MMS PDU, and this helper wraps it through the
 * wire association fixture into TPKT + COTP + X.225 session + exact X.226 Presentation.
 */
int unitlab_mms_build_wire_frame_from_pdu(
    const UnitLabMmsPdu* pdu,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_build_confirmed_response_frame(
    const UnitLabMmsPdu* response_pdu,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic);

#endif /* UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_BUILDER_H */
