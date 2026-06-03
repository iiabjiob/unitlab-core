#include "unitlab_mms_wire_builder_internal.h"

#include <stdio.h>
#include <string.h>

void wire_builder_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
{
    if (diagnostic == NULL) {
        return;
    }
    diagnostic->code = code;
    if (message == NULL) {
        diagnostic->message[0] = '\0';
        return;
    }
    snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
}

int wire_builder_encode_unsigned_integer(uint32_t value, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t bytes[5U];
    size_t length = 0U;
    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    do {
        bytes[sizeof(bytes) - 1U - length] = (uint8_t)(value & 0xFFU);
        length++;
        value >>= 8U;
    } while (value != 0U && length < sizeof(bytes));
    if (value != 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding is too large.");
        return 0;
    }
    if (bytes[sizeof(bytes) - length] & 0x80U) {
        if (length == sizeof(bytes)) {
            wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding is too large.");
            return 0;
        }
        bytes[sizeof(bytes) - length - 1U] = 0x00U;
        length++;
    }
    if (buffer_length < length) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding buffer is too small.");
        return 0;
    }
    memcpy(buffer, &bytes[sizeof(bytes) - length], length);
    *encoded_length = length;
    return 1;
}

size_t wire_builder_count_path_depth(const char* reference)
{
    size_t depth = 0U;
    int in_segment = 0;

    if (reference == NULL || reference[0] == '\0') {
        return 0U;
    }

    for (const char* cursor = reference; *cursor != '\0'; cursor++) {
        if (*cursor == '.') {
            if (in_segment) {
                depth++;
                in_segment = 0;
            }
            continue;
        }
        if (!in_segment) {
            in_segment = 1;
        }
    }
    if (in_segment) {
        depth++;
    }
    return depth;
}

size_t wire_builder_calculate_model_nesting_level(const UnitLabIedModelPlan* plan)
{
    size_t max_depth = 0U;

    if (plan == NULL || plan->signal_count == 0U || plan->signals == NULL) {
        return 5U;
    }
    for (size_t index = 0U; index < plan->signal_count; index++) {
        size_t depth = wire_builder_count_path_depth(plan->signals[index].object_reference);
        if (depth > max_depth) {
            max_depth = depth;
        }
    }
    if (max_depth == 0U) {
        return 5U;
    }
    max_depth += 2U;
    return max_depth < 5U ? 5U : max_depth;
}

uint32_t wire_builder_clamp_uint32(uint32_t value, uint32_t minimum, uint32_t maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

int wire_builder_encode_ber_element(
    UnitLabMmsBerTagClass tag_class,
    int constructed,
    uint32_t tag_number,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = tag_class;
    element.tag.constructed = constructed;
    element.tag.tag_number = tag_number;
    element.value_bytes = value_bytes;
    element.value_length = value_length;
    return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
}

int wire_builder_encode_nested_element(

    UnitLabMmsBerTagClass tag_class,
    int constructed,
    uint32_t tag_number,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    return wire_builder_encode_ber_element(tag_class, constructed, tag_number, value_bytes, value_length, buffer, buffer_length, encoded_length, diagnostic);
}

int wire_builder_append_bytes(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* offset,
    const uint8_t* bytes,
    size_t bytes_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (buffer == NULL || offset == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response builder requires valid output buffers.");
        return 0;
    }
    if (bytes_length != 0U && bytes == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Association response builder requires value bytes when length is non-zero.");
        return 0;
    }
    if (*offset > buffer_length || bytes_length > buffer_length - *offset) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Association response builder buffer is too small.");
        return 0;
    }
    if (bytes_length != 0U) {
        memcpy(&buffer[*offset], bytes, bytes_length);
    }
    *offset += bytes_length;
    return 1;
}

