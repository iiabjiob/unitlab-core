#include "server/unitlab_mms_server_runtime_internal.h"

#include <string.h>

/* Write service applies confirmed Write side effects and builds WriteResponse. */

static uint32_t server_runtime_decode_write_unsigned_bytes(const uint8_t* value_bytes, size_t value_length)
{
    uint32_t value = 0U;

    if (value_bytes == NULL || value_length == 0U) {
        return 0U;
    }
    for (size_t index = 0U; index < value_length && index < sizeof(uint32_t); index++) {
        value = (value << 8U) | (uint32_t)value_bytes[index];
    }
    return value;
}


static int server_runtime_write_reference_matches_report_control_field(const char* object_reference, const char* field_name)
{
    if (object_reference == NULL || field_name == NULL) {
        return 0;
    }
    return server_runtime_object_reference_matches_report_control_field(object_reference, field_name);
}


static int server_runtime_apply_report_control_write(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    uint32_t value = 0U;

    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required for report-control write.");
        return 0;
    }
    if (server_runtime->pending_request.kind != UNITLAB_MMS_REQUEST_WRITE) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }

    {
        size_t write_count = server_runtime->pending_request.write_object_reference_count;
        if (write_count == 0U) {
            write_count = 1U;
        }
        for (size_t index = 0U; index < write_count; index++) {
            const char* object_reference = server_runtime->pending_request.object_reference;
            const uint8_t* value_bytes = server_runtime->pending_request.write_value;
            size_t value_length = server_runtime->pending_request.write_value_length;

            if (index < server_runtime->pending_request.write_object_reference_count && server_runtime->pending_request.write_object_references[index][0] != '\0') {
                object_reference = server_runtime->pending_request.write_object_references[index];
            }
            if (index < server_runtime->pending_request.write_object_reference_count && server_runtime->pending_request.write_value_lengths[index] != 0U) {
                value_bytes = server_runtime->pending_request.write_values[index];
                value_length = server_runtime->pending_request.write_value_lengths[index];
            }
            value = server_runtime_decode_write_unsigned_bytes(value_bytes, value_length);
            if (server_runtime_write_reference_matches_report_control_field(object_reference, "ResvTms")) {
                server_runtime->brcb_resv_tms = value;
                if (value != 0U && server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
                    UnitLabMmsDiagnostic reserve_diagnostic;
                    unitlab_mms_diagnostic_clear(&reserve_diagnostic);
                    (void)unitlab_iec61850_report_control_reserve(&server_runtime->report_control, &reserve_diagnostic);
                } else if (value == 0U && server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED) {
                    UnitLabMmsDiagnostic release_diagnostic;
                    unitlab_mms_diagnostic_clear(&release_diagnostic);
                    (void)unitlab_iec61850_report_control_release(&server_runtime->report_control, &release_diagnostic);
                }
                continue;
            }
            if (server_runtime_write_reference_matches_report_control_field(object_reference, "RptEna")) {
                server_runtime->brcb_rpt_ena = value != 0U ? 1U : 0U;
                if (server_runtime->brcb_rpt_ena != 0U) {
                    UnitLabMmsDiagnostic state_diagnostic;
                    unitlab_mms_diagnostic_clear(&state_diagnostic);
                    if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
                        (void)unitlab_iec61850_report_control_reserve(&server_runtime->report_control, &state_diagnostic);
                    }
                    if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED) {
                        (void)unitlab_iec61850_report_control_enable(&server_runtime->report_control, &state_diagnostic);
                    }
                } else if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED
                           || server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING
                           || server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING) {
                    UnitLabMmsDiagnostic disable_diagnostic;
                    unitlab_mms_diagnostic_clear(&disable_diagnostic);
                    server_runtime->pending_gi_report = 0U;
                    (void)unitlab_iec61850_report_control_disable(&server_runtime->report_control, &disable_diagnostic);
                }
                continue;
            }
            if (server_runtime_write_reference_matches_report_control_field(object_reference, "GI")) {
                if (value != 0U && server_runtime->brcb_rpt_ena != 0U) {
                    UnitLabMmsDiagnostic gi_diagnostic;
                    unitlab_mms_diagnostic_clear(&gi_diagnostic);
                    if (unitlab_iec61850_report_control_request_gi(&server_runtime->report_control, &gi_diagnostic)) {
                        server_runtime->pending_gi_report = 1U;
                    }
                }
                continue;
            }

            if (server_runtime_write_reference_matches_report_control_field(object_reference, "PurgeBuf")) {
                if (value != 0U) {
                    server_runtime->brcb_sq_num = 0U;
                    server_runtime->brcb_entry_id_counter = 0U;
                    memset(server_runtime->brcb_entry_id, 0, sizeof(server_runtime->brcb_entry_id));
                    memset(server_runtime->brcb_time_of_entry, 0, sizeof(server_runtime->brcb_time_of_entry));
                    server_runtime->pending_gi_report = 0U;
                }
                continue;
            }
        }
    }

    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int server_runtime_build_write_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t member_bytes[64U];
    uint8_t response_payload_bytes[96U];
    uint8_t service_bytes[128U];
    uint8_t invoke_id_element_bytes[16U];
    size_t member_length = 0U;
    size_t response_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;
    UnitLabMmsBerElement member_element;
    UnitLabMmsBerElement response_payload_element;
    size_t write_result_count = 1U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Write response service buffer and encoded_length are required.");
        return 0;
    }
    if (!server_runtime_apply_report_control_write(server_runtime, diagnostic)) {
        return 0;
    }

    if (server_runtime->pending_request.write_object_reference_count != 0U) {
        write_result_count = server_runtime->pending_request.write_object_reference_count;
    }
    for (size_t index = 0U; index < write_result_count; index++) {
        size_t single_member_length = 0U;

        unitlab_mms_ber_element_init(&member_element);
        member_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        member_element.tag.constructed = 0;
        member_element.tag.tag_number = 1U;
        member_element.value_bytes = NULL;
        member_element.value_length = 0U;
        if (!server_runtime_encode_ber_element(
                member_element.tag.tag_class,
                member_element.tag.constructed,
                member_element.tag.tag_number,
                member_element.value_bytes,
                member_element.value_length,
                member_bytes + member_length,
                sizeof(member_bytes) - member_length,
                &single_member_length,
                diagnostic)) {
            return 0;
        }
        member_length += single_member_length;
    }

    unitlab_mms_ber_element_init(&response_payload_element);
    response_payload_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    response_payload_element.tag.constructed = 1;
    response_payload_element.tag.tag_number = 5U;
    response_payload_element.value_bytes = member_bytes;
    response_payload_element.value_length = member_length;
    if (!server_runtime_encode_ber_element(
            response_payload_element.tag.tag_class,
            response_payload_element.tag.constructed,
            response_payload_element.tag.tag_number,
            response_payload_element.value_bytes,
            response_payload_element.value_length,
            response_payload_bytes,
            sizeof(response_payload_bytes),
            &response_payload_length,
            diagnostic)) {
        return 0;
    }

    if (!server_runtime_encode_invoke_id_element(
            invoke_id,
            invoke_id_element_bytes,
            sizeof(invoke_id_element_bytes),
            &invoke_id_length,
            diagnostic)) {
        return 0;
    }

    if (invoke_id_length + response_payload_length > sizeof(service_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write response service buffer is too small.");
        return 0;
    }
    memcpy(service_bytes, invoke_id_element_bytes, invoke_id_length);
    memcpy(service_bytes + invoke_id_length, response_payload_bytes, response_payload_length);
    total_length = invoke_id_length + response_payload_length;
    if (total_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write response output buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

