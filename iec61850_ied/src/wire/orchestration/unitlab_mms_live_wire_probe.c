#include "unitlab_mms_live_wire_probe.h"

#include <string.h>

#include "wire/orchestration/unitlab_mms_association_frame.h"
#include "wire/orchestration/unitlab_mms_wire_builder_internal.h"

int unitlab_mms_build_live_wire_association_request_frame(
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsAssociationFrame fixture;
    UnitLabMmsBerElement element;
    uint8_t protocol_version_bytes[8U];
    uint8_t application_context_bytes[24U];
    uint8_t aarq_payload[32U];
    uint8_t aarq_value[24U];
    size_t protocol_version_length = 0U;
    size_t application_context_length = 0U;
    size_t aarq_value_length = 0U;
    size_t aarq_payload_length = 0U;
    const uint8_t protocol_version_value[] = { 0x00U };
    const uint8_t application_context_oid_value[] = { 0x28U, 0xCAU, 0x12U, 0x02U, 0x03U };

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            diagnostic->message[0] = '\0';
        }
        return 0;
    }

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    element.tag.constructed = 0;
    element.tag.tag_number = 0U;
    element.value_bytes = protocol_version_value;
    element.value_length = sizeof(protocol_version_value);
    if (!wire_builder_encode_ber_element(
            element.tag.tag_class,
            element.tag.constructed,
            element.tag.tag_number,
            element.value_bytes,
            element.value_length,
            protocol_version_bytes,
            sizeof(protocol_version_bytes),
            &protocol_version_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    element.tag.constructed = 0;
    element.tag.tag_number = 6U;
    element.value_bytes = application_context_oid_value;
    element.value_length = sizeof(application_context_oid_value);
    if (!wire_builder_encode_ber_element(
            element.tag.tag_class,
            element.tag.constructed,
            element.tag.tag_number,
            element.value_bytes,
            element.value_length,
            application_context_bytes,
            sizeof(application_context_bytes),
            &application_context_length,
            diagnostic)) {
        return 0;
    }

    if (protocol_version_length + application_context_length > sizeof(aarq_value)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            diagnostic->message[0] = '\0';
        }
        return 0;
    }
    memcpy(aarq_value, protocol_version_bytes, protocol_version_length);
    memcpy(aarq_value + protocol_version_length, application_context_bytes, application_context_length);
    aarq_value_length = protocol_version_length + application_context_length;

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_APPLICATION;
    element.tag.constructed = 1;
    element.tag.tag_number = 0U;
    element.value_bytes = aarq_value;
    element.value_length = aarq_value_length;
    if (!wire_builder_encode_ber_element(
            element.tag.tag_class,
            element.tag.constructed,
            element.tag.tag_number,
            element.value_bytes,
            element.value_length,
            aarq_payload,
            sizeof(aarq_payload),
            &aarq_payload_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_association_frame_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    fixture.presentation.payload_bytes = aarq_payload;
    fixture.presentation.payload_length = aarq_payload_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;
    if (!unitlab_mms_association_frame_encode(&fixture, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}
