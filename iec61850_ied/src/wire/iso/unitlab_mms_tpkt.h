#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_TPKT_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_TPKT_H

#include "../../protocols/mms/unitlab_mms_types.h"

typedef struct UnitLabMmsTpktHeader {
    uint8_t version;
    uint8_t reserved;
    uint16_t length;
} UnitLabMmsTpktHeader;

void unitlab_mms_tpkt_header_init(UnitLabMmsTpktHeader* header);
int unitlab_mms_tpkt_write_header(uint8_t* frame_bytes, size_t frame_capacity, uint16_t total_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_tpkt_wrap(const uint8_t* payload_bytes, size_t payload_length, uint8_t* frame_bytes, size_t frame_capacity, size_t* frame_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_tpkt_unwrap(const uint8_t* frame_bytes, size_t frame_length, const uint8_t** payload_bytes, size_t* payload_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

#endif
