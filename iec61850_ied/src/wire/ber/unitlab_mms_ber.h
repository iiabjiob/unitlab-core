#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_BER_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_BER_H

#include "../../protocols/mms/unitlab_mms_types.h"

typedef enum UnitLabMmsBerTagClass {
    UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL = 0,
    UNITLAB_MMS_BER_TAG_CLASS_APPLICATION = 1,
    UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC = 2,
    UNITLAB_MMS_BER_TAG_CLASS_PRIVATE = 3
} UnitLabMmsBerTagClass;

typedef struct UnitLabMmsBerTag {
    UnitLabMmsBerTagClass tag_class;
    int constructed;
    uint32_t tag_number;
} UnitLabMmsBerTag;

typedef struct UnitLabMmsBerElement {
    UnitLabMmsBerTag tag;
    const uint8_t* value_bytes;
    size_t value_length;
    size_t encoded_length;
} UnitLabMmsBerElement;

void unitlab_mms_ber_tag_init(UnitLabMmsBerTag* tag);
int unitlab_mms_ber_tag_encode(const UnitLabMmsBerTag* tag, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_ber_tag_decode(UnitLabMmsBerTag* tag, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

int unitlab_mms_ber_length_encode(size_t value_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_ber_length_decode(size_t* value_length, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);

void unitlab_mms_ber_element_init(UnitLabMmsBerElement* element);
int unitlab_mms_ber_read(UnitLabMmsBerElement* element, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsDiagnostic* diagnostic);
int unitlab_mms_ber_write(const UnitLabMmsBerElement* element, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);

#endif
