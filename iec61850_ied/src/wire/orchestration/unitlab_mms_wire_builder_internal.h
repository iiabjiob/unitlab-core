#ifndef UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_BUILDER_INTERNAL_H
#define UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_BUILDER_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "wire/ber/unitlab_mms_ber.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"
#include "model/model_plan.h"

void wire_builder_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message);
int wire_builder_encode_unsigned_integer(uint32_t value, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
size_t wire_builder_count_path_depth(const char* reference);
size_t wire_builder_calculate_model_nesting_level(const UnitLabIedModelPlan* plan);
uint32_t wire_builder_clamp_uint32(uint32_t value, uint32_t minimum, uint32_t maximum);
int wire_builder_encode_ber_element(UnitLabMmsBerTagClass tag_class, int constructed, uint32_t tag_number, const uint8_t* value_bytes, size_t value_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int wire_builder_encode_nested_element(UnitLabMmsBerTagClass tag_class, int constructed, uint32_t tag_number, const uint8_t* value_bytes, size_t value_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);
int wire_builder_append_bytes(uint8_t* buffer, size_t buffer_length, size_t* offset, const uint8_t* bytes, size_t bytes_length, UnitLabMmsDiagnostic* diagnostic);

#endif /* UNITLAB_IEC61850_IED_UNITLAB_MMS_WIRE_BUILDER_INTERNAL_H */
