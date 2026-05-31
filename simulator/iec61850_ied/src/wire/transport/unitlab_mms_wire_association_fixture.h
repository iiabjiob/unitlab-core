#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_ASSOCIATION_FIXTURE_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_ASSOCIATION_FIXTURE_H

#include "../../unitlab_mms_types.h"
#include "../presentation/unitlab_mms_presentation.h"
#include "../session/unitlab_mms_session_spdu.h"
#include "unitlab_mms_transport_frame.h"

/*
 * Narrow golden-test helper for one ISO-on-TCP association-style nested payload.
 * It composes TPKT + COTP + Session SPDU + exact X.226 Presentation User-data and leaves ACSE/MMS interpretation to the caller.
 */
typedef struct UnitLabMmsWireAssociationFixture {
    UnitLabMmsTransportFrame transport;
    UnitLabMmsSessionSpdu session;
    UnitLabMmsPresentationApdu presentation;
    size_t encoded_length;
} UnitLabMmsWireAssociationFixture;

void unitlab_mms_wire_association_fixture_init(UnitLabMmsWireAssociationFixture* fixture);
int unitlab_mms_wire_association_fixture_encode(const UnitLabMmsWireAssociationFixture* fixture, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_wire_association_fixture_decode(UnitLabMmsWireAssociationFixture* fixture, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
