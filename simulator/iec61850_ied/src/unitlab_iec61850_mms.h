#ifndef UNITLAB_IEC61850_MMS_H
#define UNITLAB_IEC61850_MMS_H

/*
 * Reusable lower-layer boundary for UnitLab IEC 61850 / MMS primitives.
 *
 * This umbrella header intentionally exposes only the owned C lower layer:
 * wire primitives, semantic PDUs, runtime bridges, and report runtime.
 */

#include "unitlab_mms_types.h"
#include "unitlab_mms_semantic_pdu.h"
#include "unitlab_mms_wire_semantic_bridge.h"
#include "unitlab_mms_runtime_bridge.h"
#include "unitlab_mms_core.h"
#include "unitlab_iec61850_report_runtime.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/iso/unitlab_mms_tpkt.h"
#include "wire/iso/unitlab_mms_cotp.h"
#include "wire/session/unitlab_mms_session_spdu.h"
#include "wire/presentation/unitlab_mms_presentation.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/transport/unitlab_mms_transport_frame.h"
#include "wire/transport/unitlab_mms_wire_association_fixture.h"

#endif /* UNITLAB_IEC61850_MMS_H */
