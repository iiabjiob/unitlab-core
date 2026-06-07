#include "server/unitlab_mms_server_runtime_internal.h"

#include <string.h>

/* Write service applies confirmed Write side effects and builds WriteResponse. */

static uint32_t server_runtime_decode_write_unsigned_value(const UnitLabMmsPendingRequest* request)
{
    uint32_t value = 0U;

    if (request == NULL || request->write_value_length == 0U) {
        return 0U;
    }
    for (size_t index = 0U; index < request->write_value_length && index < sizeof(uint32_t); index++) {
        value = (value << 8U) | (uint32_t)request->write_value[index];
    }
    return value;
}

static int server_runtime_write_target_matches_report_control_field(const UnitLabMmsPendingRequest* request, const char* field_name)
{
    if (request == NULL || field_name == NULL) {
        return 0;
    }
    return server_runtime_object_reference_matches_report_control_field(request->object_reference, field_name);
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

    value = server_runtime_decode_write_unsigned_value(&server_runtime->pending_request);
    if (server_runtime_write_target_matches_report_control_field(&server_runtime->pending_request, "ResvTms")) {
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
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (server_runtime_write_target_matches_report_control_field(&server_runtime->pending_request, "RptEna")) {
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
            (void)unitlab_iec61850_report_control_disable(&server_runtime->report_control, &disable_diagnostic);
        }
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (server_runtime_write_target_matches_report_control_field(&server_runtime->pending_request, "GI")) {
        if (value != 0U) {
            UnitLabMmsDiagnostic gi_diagnostic;
            unitlab_mms_diagnostic_clear(&gi_diagnostic);
            if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED) {
                (void)unitlab_iec61850_report_control_request_gi(&server_runtime->report_control, &gi_diagnostic);
            }
            server_runtime->pending_gi_report = 1U;
        }
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }

    if (server_runtime_write_target_matches_report_control_field(&server_runtime->pending_request, "PurgeBuf")) {
        if (value != 0U) {
            server_runtime->brcb_sq_num = 0U;
            server_runtime->brcb_entry_id_counter = 0U;
            memset(server_runtime->brcb_entry_id, 0, sizeof(server_runtime->brcb_entry_id));
            memset(server_runtime->brcb_time_of_entry, 0, sizeof(server_runtime->brcb_time_of_entry));
            server_runtime->pending_gi_report = 0U;
        }
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
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
    uint8_t member_bytes[16U];
    uint8_t response_payload_bytes[48U];
    uint8_t service_bytes[64U];
    uint8_t invoke_id_element_bytes[16U];
    size_t member_length = 0U;
    size_t response_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;
    UnitLabMmsBerElement member_element;
    UnitLabMmsBerElement response_payload_element;

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
            member_bytes,
            sizeof(member_bytes),
            &member_length,
            diagnostic)) {
        return 0;
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

